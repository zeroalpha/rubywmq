/* --------------------------------------------------------------------------
 *  Copyright 2006 J. Reid Morrison. Dimension Solutions, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * --------------------------------------------------------------------------*/

#include "ruby.h"
#include <cmqc.h>
#include <cmqxc.h>

/* Todo: Add a #ifdef here to exclude the following includes when applicable  */
#include <cmqcfc.h>                        /* PCF                             */
#include <cmqbc.h>                         /* MQAI                            */

#ifdef _WIN32
    #define WMQ_EXPORT __declspec(dllexport)
#else
    #define WMQ_EXPORT
#endif

void QueueManager_id_init();
void QueueManager_selector_id_init();
void QueueManager_command_id_init();
VALUE QUEUE_MANAGER_alloc(VALUE klass);
VALUE QueueManager_singleton_connect(int argc, VALUE *argv, VALUE self);
VALUE QueueManager_open_queue(int argc, VALUE *argv, VALUE self);
VALUE QueueManager_initialize(VALUE self, VALUE parms);
VALUE QueueManager_connect(VALUE self);
VALUE QueueManager_disconnect(VALUE self);
VALUE QueueManager_begin(VALUE self);
VALUE QueueManager_commit(VALUE self);
VALUE QueueManager_backout(VALUE self);
VALUE QueueManager_put(VALUE self, VALUE parms);
VALUE QueueManager_reason_code(VALUE self);
VALUE QueueManager_comp_code(VALUE self);
VALUE QueueManager_reason(VALUE self);
VALUE QueueManager_exception_on_error(VALUE self);
VALUE QueueManager_connected_q(VALUE self);
VALUE QueueManager_name(VALUE self);
VALUE QueueManager_execute(VALUE self, VALUE hash);

void  Queue_id_init();
VALUE QUEUE_alloc(VALUE klass);
VALUE Queue_initialize(VALUE self, VALUE parms);
VALUE Queue_singleton_open(int argc, VALUE *argv, VALUE self);
VALUE Queue_open(VALUE self);
VALUE Queue_close(VALUE self);
VALUE Queue_put(VALUE self, VALUE parms);
VALUE Queue_get(VALUE self, VALUE parms);
VALUE Queue_each(int argc, VALUE *argv, VALUE self);
VALUE Queue_name(VALUE self);
VALUE Queue_reason_code(VALUE self);
VALUE Queue_comp_code(VALUE self);
VALUE Queue_reason(VALUE self);
VALUE Queue_open_q(VALUE self);

extern VALUE wmq_queue;
extern VALUE wmq_queue_manager;
extern VALUE wmq_message;
extern VALUE wmq_exception;

#define WMQ_EXEC_STRING_INQ_BUFFER_SIZE 32768           /* Todo: Should we make the mqai string return buffer dynamic? */

/* Internal C Structures for holding MQ data types */
 typedef struct tagQUEUE_MANAGER QUEUE_MANAGER;
 typedef QUEUE_MANAGER MQPOINTER PQUEUE_MANAGER;

 struct tagQUEUE_MANAGER {
    MQHCONN  hcon;                    /* connection handle             */
    MQLONG   comp_code;               /* completion code               */
    MQLONG   reason_code;             /* reason code for MQCONN        */
    MQLONG   exception_on_error;      /* Non-Zero means throw exception*/
    MQLONG   already_connected;       /* Already connected means don't disconnect */
    MQLONG   trace_level;             /* Trace level. 0==None, 1==Info 2==Debug ..*/
    MQCNO    connect_options;         /* MQCONNX Connection Options    */
  #ifdef MQCNO_VERSION_2
    MQCD     client_conn;             /* Client Connection             */
  #endif
  #ifdef MQCNO_VERSION_4
    MQSCO    ssl_config_opts;         /* Security options              */
  #endif
  #ifdef MQCD_VERSION_6
    MQPTR    long_remote_user_id_ptr;
  #endif
  #ifdef MQCD_VERSION_7
    MQPTR    ssl_peer_name_ptr;
  #endif
  #ifdef MQHB_UNUSABLE_HBAG
    MQHBAG   admin_bag;
    MQHBAG   reply_bag;
  #endif
    PMQBYTE  p_buffer;                /* message buffer                */
    MQLONG   buffer_size;             /* Allocated size of buffer      */
 };

/*
 * Message
 */
void  Message_id_init();
VALUE Message_initialize(int argc, VALUE *argv, VALUE self);
VALUE Message_clear(VALUE self);

void Message_build(PMQBYTE* pq_pp_buffer, PMQLONG pq_p_buffer_size, MQLONG trace_level,
                   VALUE parms, PPMQVOID pp_buffer, PMQLONG p_total_length, PMQMD pmqmd);
void Message_build_mqmd(VALUE self, PMQMD pmqmd);
void Message_deblock(VALUE message, PMQMD pmqmd, PMQBYTE p_buffer, MQLONG total_length);

struct Message_build_header_arg {
    PMQBYTE* pp_buffer;                               /* Autosize: Pointer to start of total buffer */
    PMQLONG  p_buffer_size;                           /* Autosize: Size of total buffer */
    MQLONG   data_length;                             /* Autosize: Length of the data being written */

    PMQLONG  p_data_offset;                           /* Current offset of data portion in total buffer */
    MQLONG   trace_level;                             /* Trace level. 0==None, 1==Info 2==Debug ..*/
};

int  Message_build_header(VALUE hash, struct Message_build_header_arg* parg);

/* Utility methods */

/* --------------------------------------------------
 * Set internal variable based on value passed in from
 * a hash
 * --------------------------------------------------*/
void setFromHashString(VALUE self, VALUE hash, char* pKey, char* pAttribute, char* pDefault);
void setFromHashValue(VALUE self, VALUE hash, char* pKey, char* pAttribute, VALUE valDefault);

void to_mqmd(VALUE hash, MQMD* pmqmd);
void from_mqmd(VALUE hash, MQMD* pmqmd);

void to_mqdlh(VALUE hash, MQDLH* pmqdlh);
void from_mqdlh(VALUE hash, MQDLH* pmqdlh);

void wmq_structs_id_init();

char*  wmq_reason(MQLONG reason_code);
ID     wmq_selector_id(MQLONG selector);
MQLONG wmq_command_lookup(ID command_id);
void   wmq_selector(ID selector_id, PMQLONG selector_type, PMQLONG selector);

/* --------------------------------------------------
 * MACROS for moving data between Ruby and MQ
 * --------------------------------------------------*/
#define WMQ_HASH2MQLONG(HASH,KEY,ELEMENT)       \
    val = rb_hash_aref(HASH, ID2SYM(ID_##KEY)); \
    if (!NIL_P(val)) { ELEMENT = NUM2LONG(val); }

#define WMQ_STR2MQCHARS(STR,ELEMENT)            \
    str = StringValue(STR);                     \
    length = RSTRING(STR)->len;                 \
    size = sizeof(ELEMENT);                     \
    strncpy(ELEMENT, RSTRING(STR)->ptr, length > size ? size : length);

#define WMQ_HASH2MQCHARS(HASH,KEY,ELEMENT)      \
    val = rb_hash_aref(HASH, ID2SYM(ID_##KEY)); \
    if (!NIL_P(val))                            \
    {                                           \
         WMQ_STR2MQCHARS(val,ELEMENT)           \
    }

#define WMQ_HASH2MQCHAR(HASH,KEY,ELEMENT)      \
    val = rb_hash_aref(HASH, ID2SYM(ID_##KEY)); \
    if (!NIL_P(val)) { ELEMENT = NUM2LONG(val); }

#define WMQ_HASH2MQBYTES(hash,KEY,ELEMENT)      \
    val = rb_hash_aref(hash, ID2SYM(ID_##KEY)); \
    if (!NIL_P(val))                            \
    {                                           \
         str = StringValue(val);                \
         length = RSTRING(str)->len;            \
         size = sizeof(ELEMENT);                \
         if (length >= size)                    \
         {                                      \
            memcpy(ELEMENT, RSTRING(str)->ptr, size);   \
         }                                              \
         else                                           \
         {                                              \
            memcpy(ELEMENT, RSTRING(str)->ptr, length); \
            memset(ELEMENT+length, 0, size-length);     \
         }                                              \
    }

#define WMQ_HASH2BOOL(HASH,KEY,ELEMENT)             \
    val = rb_hash_aref(hash, ID2SYM(ID_##KEY));     \
    if (!NIL_P(val))                                \
    {                                               \
        if(TYPE(val) == T_TRUE)       ELEMENT = 1;  \
        else if(TYPE(val) == T_FALSE) ELEMENT = 0;  \
        else                                        \
             rb_raise(rb_eTypeError, ":" #KEY       \
                         " must be true or false"); \
    }

#define IF_TRUE(KEY,DEFAULT)                        \
    val = rb_hash_aref(hash, ID2SYM(ID_##KEY));     \
    if (NIL_P(val))                                 \
    {                                               \
        flag = DEFAULT;                             \
    }                                               \
    else                                            \
    {                                               \
        if(TYPE(val) == T_TRUE)       flag = 1;     \
        else if(TYPE(val) == T_FALSE) flag = 0;     \
        else                                        \
             rb_raise(rb_eTypeError, ":" #KEY       \
                         " must be true or false"); \
    }                                               \
    if (flag)

/* --------------------------------------------------
 * Strip trailing nulls and spaces
 * --------------------------------------------------*/
#define WMQ_MQCHARS2STR(ELEMENT, TARGET) \
    size = sizeof(ELEMENT);           \
    length = 0;                       \
    pChar = ELEMENT + size-1;         \
    for (i = size; i > 0; i--)        \
    {                                 \
        if (*pChar != ' ' && *pChar != 0) \
        {                             \
            length = i;               \
            break;                    \
        }                             \
        pChar--;                      \
    }                                 \
    TARGET = rb_str_new(ELEMENT,length);

#define WMQ_MQCHARS2HASH(HASH,KEY,ELEMENT)        \
    WMQ_MQCHARS2STR(ELEMENT, str)                 \
    rb_hash_aset(HASH, ID2SYM(ID_##KEY), str);

#define WMQ_MQLONG2HASH(HASH,KEY,ELEMENT) \
    rb_hash_aset(HASH, ID2SYM(ID_##KEY), LONG2NUM(ELEMENT));

#define WMQ_MQCHAR2HASH(HASH,KEY,ELEMENT) \
    rb_hash_aset(HASH, ID2SYM(ID_##KEY), LONG2NUM(ELEMENT));

/* --------------------------------------------------
 * Trailing Spaces are important with binary fields
 * --------------------------------------------------*/
#define WMQ_MQBYTES2STR(ELEMENT,TARGET)  \
    size = sizeof(ELEMENT);           \
    length = 0;                       \
    pChar = ELEMENT + size-1;         \
    for (i = size; i > 0; i--)        \
    {                                 \
        if (*pChar != 0)              \
        {                             \
            length = i;               \
            break;                    \
        }                             \
        pChar--;                      \
    }                                 \
    TARGET = rb_str_new(ELEMENT,length);

#define WMQ_MQBYTES2HASH(HASH,KEY,ELEMENT)  \
    WMQ_MQBYTES2STR(ELEMENT, str)           \
    rb_hash_aset(HASH, ID2SYM(ID_##KEY), str);

