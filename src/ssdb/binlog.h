/*
  Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/
#ifndef SSDB_BINLOG_H_
#define SSDB_BINLOG_H_

#include <string>
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "rocksdb/write_batch.h"
#include "../util/thread.h"
#include "../util/bytes.h"


class Binlog{
 private:
    std::string buf;
    static const unsigned int HEADER_LEN = sizeof(uint64_t) + 2;
 public:
    Binlog(){}
    Binlog(uint64_t seq, char type, char cmd, const rocksdb::Slice &key);
		
    int load(const Bytes &s);
    int load(const rocksdb::Slice &s);
    int load(const std::string &s);

    uint64_t seq() const;
    char type() const;
    char cmd() const;
    const Bytes key() const;

    const char* data() const{
	return buf.data();
    }
    int size() const{
	return (int)buf.size();
    }
    const std::string repr() const{
	return this->buf;
    }
    std::string dumps() const;
};

// circular queue
class BinlogQueue{
 private:
    rocksdb::DB *db;
    rocksdb::WriteOptions _write_opts;
    uint64_t _min_seq;
    uint64_t _last_seq;
    uint64_t _tran_seq;
    int _capacity;
    std::vector<rocksdb::ColumnFamilyHandle*> _cfHandles;
    enum {
	kDefaultCFHandle = 0,
	kOplogCFHandle = 1
    };
    rocksdb::WriteBatch _batch;

    volatile bool thread_quit;
    static void* log_clean_thread_func(void *arg);
    int del(uint64_t seq);
    // [start, end] includesive
    int del_range(uint64_t start, uint64_t end);
	
    void clean_obsolete_binlogs();
    void merge();
    bool enabled;

 public:
    Mutex mutex;

    BinlogQueue(rocksdb::DB *db, std::vector<rocksdb::ColumnFamilyHandle*> handles,
		bool enabled=true, int capacity=20000000);
    ~BinlogQueue();
    void begin();
    void rollback();
    rocksdb::Status commit();
    // rocksdb put
    void Put(const rocksdb::Slice& key, const rocksdb::Slice& value);
    // rocksdb merge
    void Merge(const rocksdb::Slice& key, const rocksdb::Slice& value);
    // rocksdb delete
    void Delete(const rocksdb::Slice& key);
    
    void add_log(char type, char cmd, const rocksdb::Slice &key);
    void add_log(char type, char cmd, const std::string &key);
		
    int get(uint64_t seq, Binlog *log) const;
    int update(uint64_t seq, char type, char cmd, const std::string &key);
		
    void flush();
		
    /** @returns
	1 : log.seq greater than or equal to seq
	0 : not found
	-1: error
    */
    int find_next(uint64_t seq, Binlog *log) const;
    int find_last(Binlog *log) const;
	
    uint64_t min_seq() const{
	return _min_seq;
    }
    uint64_t max_seq() const{
	return _last_seq;
    }
		
    std::string stats() const;
};

class Transaction{
 private:
    BinlogQueue *logs;
 public:
    Transaction(BinlogQueue *logs){
	this->logs = logs;
	logs->mutex.lock();
	logs->begin();
    }
	
    ~Transaction(){
	// it is safe to call rollback after commit
	logs->rollback();
	logs->mutex.unlock();
    }
};


#endif
