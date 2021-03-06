/**
 * PLAIN FRAMEWORK ( https://github.com/viticm/plainframework )
 * $Id nodelogic.h
 * @link https://github.com/viticm/plainframework for the canonical source repository
 * @copyright Copyright (c) 2014- viticm( viticm.ti@gmail.com )
 * @license
 * @user viticm<viticm.ti@gmail.com>
 * @date 2014/07/31 10:41
 * @uses share memory archive node logic class
 */
#ifndef ARCHIVE_NODE_LOGIC_H_
#define ARCHIVE_NODE_LOGIC_H_

#include "archive/node/config.h"
#include "pf/base/time_manager.h"
#include "pf/base/log.h"
#include "pf/sys/memory/share.h"

namespace archive {

namespace node {

template <typename T>
class Logic {

 public:
   Logic() {
     pool_ = NULL;
     type_ = common::sharememory::kTypeInvaild;
     final_savetime_ = 0;
   }
   
   ~Logic() {
     SAFE_DELETE(pool_);
   }
   
 public:
   bool init(int32_t sizemax) {
     pool_->init(sizemax, 
                 data_.key, 
                 pf_sys::memory::share::kSmptShareMemory);
     pool_->set_head_version(0);
     last_checktime_ = TIME_MANAGER_POINTER->get_tickcount();
     lastversion_ = 0;
     bool result = init_after();
     return result;
   }

 public:
   bool init_after();
   bool tick() {
     __ENTER_FUNCTION
       uint32_t time = TIME_MANAGER_POINTER->get_tickcount();
       if (fabs(static_cast<double>(time - last_checktime_)) > 
           ARCHIVE_DETECT_IDLE) {
         last_checktime_ = time;
         uint32_t version = pool_->get_head_version();
         if (version == lastversion_ && lastversion_ > 0) {
           lastversion_ = 0;
           SLOW_LOG(APPLICATION_NAME, "receive server crash command.");
           bool result = fullflush(true, true);
           pool_->set_head_version(0);
           return result;
         }
       }
       if (g_commond_exit) {
         fullflush(true);
       } else {
         tickflush();
       }
       return true;
     __LEAVE_FUNCTION
       return false; 
   }
   bool empty();
   bool fullflush(bool force, bool servercrash = false);
   bool tickflush();

 public:
   void setdata(common::share_memory_data_t &data) {
     data_ = data;
   }
   common::share_memory_data_t getdata() const {
     return data_;
   }
   void settype(common::sharememory::type_t type) {
     type_ = type;
   }
   common::sharememory::type_t gettype() const {
     return type_;
   }
   void setpool(pf_sys::memory::share::UnitPool<T> *pool) {
     pool_ = pool;
   }
   pf_sys::memory::share::UnitPool<T> *getpool() {
     return pool_;
   }

 private:
   pf_sys::memory::share::UnitPool<T> *pool_;
   common::share_memory_data_t data_;
   common::sharememory::type_t type_;
   uint32_t final_savetime_;
   bool isready_;
   uint32_t lastversion_;
   uint32_t last_checktime_;

};

}; //namespace node

}; //namespace archive

#endif //ARCHIVE_NODE_LOGIC_H_
