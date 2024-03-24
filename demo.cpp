#include <assert.h>
#include <iostream>
#include <string>

#include "leveldb/db.h"

using namespace std;

int main(void) {
  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;

  // open
  leveldb::Status status = leveldb::DB::Open(options, "testdb", &db);
  assert(status.ok());

  string key = "name";
  string value = "shane";

  cout << "write:"
          "("
       << key
       << ")"
          "=>"
          "("
       << value << ")" << endl;
  // write
  status = db->Put(leveldb::WriteOptions(), key, value);
  assert(status.ok());

  // read
  status = db->Get(leveldb::ReadOptions(), key, &value);
  assert(status.ok());

  cout << "read:"
          "("
       << key
       << ")"
          "=>"
          "("
       << value << ")" << endl;

  // delete
  status = db->Delete(leveldb::WriteOptions(), key);
  assert(status.ok());
  cout << "delete:"
          "("
       << key << ")" << status.ToString() << endl;

  status = db->Get(leveldb::ReadOptions(), key, &value);
  if (!status.ok()) {
    cout << "read error: " << status.ToString() << "(" << key << ")" << endl;
  } else {
    cout << "read:"
            "("
         << key
         << ")"
            "=>"
            "("
         << value << ")" << endl;
  }

  // close
  delete db;

  return 0;
}