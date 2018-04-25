//
// Created by JinAobo on 3/20/18.
//

#include <iostream>
#include <stdlib.h>
#include <string>
#include "SQLParser.h"
#include "sqlhelper.h"
#include <sstream>
#include <iterator>
#include <cstring>
#include <map>
#include <vector>
#include <fstream>
#include "table.h"
#include "util.h"
#include <pthread.h>
#include "util.h"
#include "ThreadPool.cpp"
#include <algorithm>




using namespace std;

void executeStatement(hsql::SQLStatement *stmt, map<string, table*> &table_list);
void createTable(hsql::CreateStatement *stmt, map<string, table*> &table_list);
bool insertTable(hsql::InsertStatement *stmt, map<string, table*> &table_list, bool check);
void executeShow(hsql::ShowStatement *stmt, map<string, table*> &table_list);
void executeSelect(hsql::SelectStatement *stmt,  map<string, table*> &table_list);
bool executeUpdate(hsql::UpdateStatement *stmt, map<string, table*> &table_list, bool check);
void joinTable(table* t1, table* t2, hsql::SelectStatement *stmt);
void executeTransaction(string transbuffer, vector<string> lockitem);

void loadFromFile(map<string, table*> &map_list);
void saveToFile(map<string, table*> &map_list);
void printTableList(map<string, table*> table_list);
void loadTableList(map<string, table*> & table_list);
inline std::vector<std::string> split(const std::string &s, char delim);
void Lowercase(string &s);
string removespace(string s);


map<string, int> locks;
map<string, table*> table_list;

int main(int argc, char * argv[]){

    if(argc <= 1){
        cout<<"please input query to start or use script=filename to start SQL!"<<endl;
        exit(1);
    }
    string query = "";
    for(int i = 1 ; i < argc; i ++ ){
        query += argv[i];
        query += " ";
    }



    loadTableList(table_list);

    if(query.find("script=")== 0)
    {
        //transcation part
        if(query.find("numthreads")){
            cout<<"do transaction"<<endl;
            string temp = "";
            string filename = "";
            string pkcolumn = "";
            int num_threads = 10;

            for(auto p : query.substr(7)){

                if(p == ';' | p == ' '){
                    if(filename == ""){
                        filename = temp;
                    }
                    else if(temp.find("numthreads") == 0){
                        num_threads = stoi(temp.substr(11));
                    }
                    else if(temp.find("pkcolumn") == 0){
                        pkcolumn = temp.substr(9);
                    }
                    temp = "";
                }
                else
                    temp += p;

            }
            temp = "";


            //read script sql build thread

            vector<string> lockitem;


            ThreadPool threads(num_threads);
            string transbuffer="";
            //read .sql file to transcations
            ifstream file(filename);
            if(!file.is_open()){
                cout<< "file cannot opened"<<endl;
            }
            string buffer="";
            while(getline(file, temp)){
                string rs_temp = removespace(temp);
                if(rs_temp.length() <= 1)
                    continue;
                Lowercase(temp);
                //if(util::compareString(temp.substr(temp.length()-1), ";") == 1){
                if(temp.find(";") != string::npos){
                    buffer += " " + temp;
                    //deal with transcation
                    if(buffer.find("begin transaction") != string::npos){
                        string t_temp = "";
                        //filter out BEGIN TRANSACTION
                        transbuffer = buffer.substr(18);
                        while(getline(file, t_temp)){
                            rs_temp = removespace(t_temp);
                            if(rs_temp.length() <= 1)
                                continue;
                            Lowercase(t_temp);
                            transbuffer = transbuffer +  " " + t_temp;
                            //cout << transbuffer << endl;
                            if(t_temp.find("where") != string::npos){
                                string substring = t_temp.substr(t_temp.find("where") + 6);
                                substring = substring.substr(0, substring.find(";"));
                                vector<string> split_string = split(substring, '=');
                                lockitem.push_back(removespace(split_string[0]) + removespace(split_string[1]));

                            }
                            if(transbuffer.find("end transaction") != string::npos){
                                cout << "execute transaction" << endl;
                                cout << transbuffer << endl;
                                cout << "length: " << transbuffer.length() << endl;
                                //threads.doJob(bind(executeTransaction, transbuffer, lockitem));
                                transbuffer = "";
                                break;
                            }
                        }

                    }
                    else{
                        //execute query directly
                        hsql::SQLParserResult *result = hsql::SQLParser::parseSQLString(buffer);

                        // check whether the parsing was successful
                        if (result->isValid()) {
                            for (unsigned i = 0; i < result->size(); ++i) {
                                //run sql query
                                executeStatement(result->getMutableStatement(i), table_list);
                                saveToFile(table_list);

                            }
                        } else {
                            cout << "Given string is not a valid SQL query." << endl
                                 << result->errorMsg() << "(" << result->errorLine() << ":" << result->errorColumn() << ")" << endl;
                        }
                    }

                    buffer = "";
                } else
                    buffer += " " + temp;

            }


            return 0;
        }

        //read script and run sql by script
        cout << "run script " << endl;
        ifstream file(query.substr(7, query.length()-8));
        if(!file.is_open())
        {
            cout << "file cannot opened" <<endl;
        }
        stringstream buffer;
        buffer << file.rdbuf();
        string wholequery = "";
        for(auto c : buffer.str()){
            if(c != '\r' )
                wholequery += c;
        }
        hsql::SQLParserResult *result = hsql::SQLParser::parseSQLString(wholequery);
        // check whether the parsing was successful
        if (result->isValid()) {
            for (unsigned i = 0; i < result->size(); ++i) {
                //run sql query
                executeStatement(result->getMutableStatement(i), table_list);
                saveToFile(table_list);

            }
        } else {
            cout << "Given string is not a valid SQL query." << endl
                 << result->errorMsg() << "(" << result->errorLine() << ":" << result->errorColumn() << ")" << endl;
        }

        return 0;


    }
    else{
        query += ";";
        while(true) {

            // printTableList(table_list);

            hsql::SQLParserResult *result = hsql::SQLParser::parseSQLString(query);
            // check whether the parsing was successful
            if (result->isValid()) {
                for (unsigned i = 0; i < result->size(); ++i) {
                    //run sql query
                    executeStatement(result->getMutableStatement(i), table_list);
                    saveToFile(table_list);

                }
            } else {
                cout << "Given string is not a valid SQL query." << endl
                     << result->errorMsg() << "(" << result->errorLine() << ":" << result->errorColumn() << ")" << endl;
            }
            query.clear();
            cout << "SQL>";


            while(query.find(";") == string::npos){
                string line;
                getline(cin, line);
                query += line;
            }
            if (query == "quit;") {
                saveToFile(table_list);
                exit(0);
            }
        }
    }

    return 0;
}

void executeStatement(hsql::SQLStatement *stmt, map<string, table*> &table_list){
    switch (stmt->type()) {
        case hsql::kStmtCreate:
            cout << "Create" <<endl;
            createTable((hsql::CreateStatement*)stmt, table_list);
            break;
        case hsql::kStmtSelect:
            cout << "Select" <<endl;
            executeSelect((hsql::SelectStatement*)stmt, table_list);
            break;
        case hsql::kStmtInsert:
            cout << "Insert" <<endl;
            insertTable((hsql::InsertStatement*)stmt, table_list, false);
            break;
        case hsql::kStmtShow:
            cout << "Show" <<endl;
            executeShow((hsql::ShowStatement*)stmt, table_list);
            break;
        case hsql::kStmtUpdate:
            cout << "Updaet" <<endl;
            executeUpdate((hsql::UpdateStatement*)stmt, table_list, false);
        default:
            break;
    }
}



void printTableList(map<string, table*> table_list){

    if(table_list.empty()) cout << "nothing" << endl;
    else cout << table_list.begin()->first <<endl;

    for(map<string, table*>::const_iterator it = table_list.begin();
        it != table_list.end(); ++it)
    {
        std::cout << it->first << " " << it->second->getRowlength() << " " << it->second->getPrimaryKey() << "\n";
    }
}


void createTable(hsql::CreateStatement *stmt, map<string, table*> &table_list){
    cout << "Creating table " << stmt->tableName << "... " <<endl;
    //chect duplicate columns
    vector<char*> colnames;
    for(hsql::ColumnDefinition* col_def: *stmt->columns){
        for(auto colname:colnames){
            if(util::compareString(colname, col_def->name)){
                cout<<"Can't create table with duplicate column names"<<endl;
                return;
            }
        }
        colnames.push_back(col_def->name);
    }
    //check if table exist
    for(auto t:table_list){
        if(util::compareString(t.second->getabsName(), stmt->tableName)){
            cout << "table "<<stmt->tableName << " already exists"<<endl;
            return;
        }
    }
    table* newtable = new table(stmt->tableName);
    vector<column* > cols;
    //put cols to table
    int row_length = 0;
    for(hsql::ColumnDefinition* col_def : *stmt->columns){
        string flag ;
        int size = 0;
        if (col_def-> type == hsql::ColumnDefinition::DataType::INT){
            flag = "INT";
            size = 8;
            row_length += 8;
        }
        else if (col_def->type == hsql::ColumnDefinition::DataType::TEXT){
            flag = "CHAR";
            size = col_def->size;
            row_length += size;
            row_length += 1;
        }
        column* newcol = new column(col_def->name, flag, size);
        cols.push_back(newcol);
    }
    newtable->addColumn(cols);
    newtable->setRowSize(row_length);
    ofstream os(newtable->getName(), ios::out | ios::binary);

    table_list.insert(make_pair(stmt->tableName, newtable));

    //set primary key
    if(stmt->primaryKey != NULL){
        newtable->setPrimaryKey(stmt->primaryKey->name);
    }
    os.close();

}
bool insertTable(hsql::InsertStatement *stmt, map<string, table*> &table_list, bool check){
    cout<<"Insert into table : "<< stmt->tableName << endl;

    table* totable = util::getTable(stmt->tableName, table_list);
    if(totable == NULL){
        cout<<"table "<<stmt->tableName<<" not exits"<<endl;
        return false;
    }

    if (stmt->type == hsql::InsertStatement::kInsertValues&& !check){
        if(totable->insert(stmt)){
            cout << "insert successful" << endl;
            return true;
        }
    }
    //check insert
    if (stmt->type == hsql::InsertStatement::kInsertValues&&check){
        if(totable->insertcheck(stmt)){
            return true;
        }
    }

    if (stmt->type == hsql::InsertStatement::kInsertSelect){
        if(totable->insertSelect(stmt, table_list)){
            cout << "insert successful" << endl;
            return true;
        }
    }
    return false;

}


void executeShow(hsql::ShowStatement *stmt, map<string, table*> &table_list){
    ifstream is("CATALOG.txt");
    string line;
    if(stmt->tableName ){
        while(getline(is, line)){
                if (stmt->tableName == line.substr(10)){
                    cout << "Table " << line.substr(10) << "(" ;
                    // columns
                    getline(is, line);
                    cout << line.substr(8) ;
                    //primaryKey
                    getline(is, line);
                    cout << "," << line << ")"<< endl;
                    getline(is,line);
                    getline(is, line);
                    getline(is, line);
                }
                else{
                    getline(is, line);
                    getline(is, line);
                    getline(is, line);
                    getline(is, line);
                    getline(is, line);
                }
        }
    }
    else {
        while(getline(is, line)){
            //table name
            cout << "Table " << line.substr(10) << "(" ;
            // columns
            getline(is, line);
            cout << line.substr(8) ;
            //primaryKey
            getline(is, line);
            cout << "," << line << ")"<< endl;
            //recordsize
            getline(is,line);
            // total size
            getline(is, line);
            // records
            getline(is, line);
        }
    }

}

void executeSelect(hsql::SelectStatement *stmt, map<string, table*> &table_list){
    if(stmt->fromTable->type == hsql::kTableName){
        table* totable = util::getTable(stmt->fromTable->name, table_list);
        if(totable == NULL){
            cout<< "did not find table " << stmt->fromTable->name << " from database"<<endl;
            return;
        }

        if(totable != nullptr) {
            //totable->select(stmt);
            util::PrintRecords(stmt, totable->select(stmt), totable);
        }
    }
    else{//do join
        cout<< "join" <<endl;
        char* leftname = stmt->fromTable->join->left->getName();
        char* rightname =  stmt->fromTable->join->right->getName();
        table* lefttable = util::getTable(leftname, table_list);
        table* righttable = util::getTable(rightname, table_list);
        if(lefttable==nullptr)
            cout << "table "<< leftname << " do not exist" << endl;
        if (righttable == nullptr)
            cout << "table "<< rightname << " do not exist" << endl;
        if(lefttable != nullptr && righttable != nullptr){
            //join
            joinTable(lefttable, righttable, stmt);
        }

    }

}
void joinTable(table* t1, table* t2, hsql::SelectStatement *stmt){

    vector<pair<string, column*>> cols_left;
    vector<pair<string, column*>> cols_right;

    cols_left = t1->select(stmt);
    cols_right = t2->select(stmt);

    util::PrintJoinRecords(stmt, cols_left, cols_right, t1, t2);


}

bool executeUpdate(hsql::UpdateStatement *stmt, map<string, table*> &table_list, bool check){
    if(stmt->table->type == hsql::kTableName) {
        table *totable = util::getTable(stmt->table->name, table_list);
        if (totable == NULL) {
            cout << "did not find table " << stmt->table->name << " from database" << endl;
            return false;
        }
        if(totable != nullptr){

            if(totable->update(stmt)&&!check){
                cout << "update successful" << endl;
                return true;
            }
            if(totable->updatecheck(stmt)&&check){
                return true;
            }

        }
    }
    cout << "update false"<<endl;
    return false;
}

//need more founctions

void saveToFile(map<string, table*> &map_list){
    if(map_list.size() == 0){
        return;
    }
    ofstream os("CATALOG.txt");
    for(auto tl: map_list){
        os << "tablename=";
        os << tl.first <<endl;
        os << "columns=";
        string  temp_str = "";
        for(auto col : tl.second->table_cols){
            if(col->flag == "INT") temp_str += col->name + ":" + col->flag + ",";
            else temp_str += col->name + ":" + col->flag + "("+ to_string( col->element_size) + "),";
        }
        os << temp_str.substr(0, temp_str.size() - 1 ) << endl;

        if(tl.second->getPrimaryKey()!= NULL)
            os << "primaryKey=" << tl.second->getPrimaryKey()->name << endl;
        else
            os << "primaryKey=NULL" << endl;
        os << "recordsize="<<tl.second->getRecordSize() << endl;
        os << "totalrecordsize="<<tl.second->getTotalRecordSize() << endl;
        os << "records="<< tl.second->getRowlength()<< endl;
    }
    os.close();
}

void loadTableList(map<string, table*> &table_list){
    ifstream is("CATALOG.txt");
    string line;
    while(getline(is, line)){
        string tableName = line.substr(10);
        table* t = new table(tableName );
       // cout << line.substr(10) << endl;
        getline(is, line);

        vector<column*> columns;
        string strCol = line.substr(8);
        vector<string> tokens = split(strCol, ',');
        int rowSize = 0;
        for(auto col_str:tokens) {
            column *col = new column();
            col->fromString(col_str);
            rowSize += col->element_truesize;
            columns.push_back(col);
        }
        t->addColumn(columns);

        // primary key
        getline(is, line);
        t->setPrimaryKey(line.substr(11));

        getline(is, line);
        // total size
        getline(is, line);

        // records
        getline(is, line);
        t->setRowLength(stoi(line.substr(8)));

        t->setRowSize(rowSize);

        table_list.insert(make_pair(tableName, t));
    }

    is.close();
}

inline std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}
void Lowercase(string &s){
    //for (int i=0; s[i]; i++) s[i] = tolower(s[i]);
    int i = 0;
    char c;
    while(s[i]){
        if(s[i] <='Z' && s[i] >= 'A'){
            c = s[i];
            s[i] = tolower(c);
            i++;
            continue;
        }
        if(s[i] == '\r')
            s[i] = ' ';
        i ++;
    }
}
string removespace(string s){
    string temp;
    for (int i=0; s[i];i++){
        if(s[i] == ' ' )
            continue;
        temp += s[i];
    }
    return temp;
}

void executeTransaction(string transbuffer, vector<string> lockitem){
    //lock on the record
    for(auto item: lockitem){
        while(true){
            if(locks.find(item) != locks.end() && locks[item] == 0)
                break;
            if(locks.find(item) == locks.end())
                break;
        }
    }
    //add locd to locks
    for(auto item: lockitem){
        if(locks.find(item) != locks.end())
            locks[item] = 1;
        else
            locks.insert(make_pair(item, 1));
    }

    //check if the transbuffer can be executed.

    vector<string> querys = split(transbuffer, ';');
    bool cancommit = true;

    for(auto query : querys){
        cout << "querys:::::::::::: " << query << endl;
        if(query.find("end transaction") != string::npos || query.find("commit") != string::npos)
            continue;

        hsql::SQLParserResult *result = hsql::SQLParser::parseSQLString(query);
        // check whether the parsing was successful
        if (result->isValid()) {
            for (unsigned i = 0; i < result->size(); ++i) {
                //run sql query


                //check insert
                if(query.find("insert") != string::npos){
                    hsql::InsertStatement *stmt = (hsql::InsertStatement*)result->getMutableStatement(i);
                    table* totable = util::getTable(stmt->tableName, table_list);
                    if(totable == NULL)
                        cancommit = false;
                    if(totable != NULL && !totable->insertcheck(stmt))
                        cancommit = false;

                }

                if(query.find("update") != string::npos){
                    hsql::UpdateStatement *stmt = (hsql::UpdateStatement*)result->getMutableStatement(i);
                    table* totable = util::getTable(stmt->table->name, table_list);
                    if(totable == NULL)
                    {
                        cout << "cannot update this statement"<< endl;
                        cancommit = false;
                    }
                    if(totable != NULL && !totable->updatecheck(stmt))
                    {
                        cout << "cannot update this statement"<< endl;
                        cancommit = false;
                    }

                }


            }
        } else {
            //split update to two querys and check again
            if(query.find("update") != string::npos){
                //cout << "find updated" << query << endl;
                string select_item = query.substr(query.find("set") + 3, query.find("where") - query.find("set") - 3);
                select_item = split(select_item, '=')[0];
                string table = query.substr(query.find("update") + 6, query.find("set") - query.find("update") -6);
                string where = query.substr(query.find("where") + 5, query.find(";") - query.find("where") - 5);

                string select_query = "select " + select_item + " from " + table + " where " + where + ";";
                string update_query = "update " + table + " set " + select_item + " = ?" + " where " + where + ";";
                //cout << "((((((((((((((("<<endl;
                //cout << select_query<< endl;
                //cout << update_query<<endl;

            }

            cout << "Given string is not a valid SQL query." << endl
                 << result->errorMsg() << "(" << result->errorLine() << ":" << result->errorColumn() << ")" << endl;
            cancommit = false;
        }

        cout << "find query" << query << endl;
        if(cancommit == false)
            break;
    }

    //commit
    if(cancommit){

    }




    //execute the transbuffer


    //unlock
    for(auto item:lockitem){
        locks[item] = 0;
    }

}
