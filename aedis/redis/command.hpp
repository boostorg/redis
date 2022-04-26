/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_REDIS_COMMAND_HPP
#define AEDIS_REDIS_COMMAND_HPP

#include <ostream>
#include <string>

namespace aedis {
namespace redis {

/** \brief Redis commands.
 *  \ingroup any
 *
 *  For a full list of commands see https://redis.io/commands.
 *
 *  \remark The list of commands below are read from Redis with the
 *  help of the command \c command.
 */
enum class command {
   /// https://redis.io/commands/acl
   acl,
   /// https://redis.io/commands/append
   append,
   /// https://redis.io/commands/asking
   asking,
   /// https://redis.io/commands/auth
   auth,
   /// https://redis.io/commands/bgrewriteaof
   bgrewriteaof,
   /// https://redis.io/commands/bgsave
   bgsave,
   /// https://redis.io/commands/bitcount
   bitcount,
   /// https://redis.io/commands/bitfield
   bitfield,
   /// https://redis.io/commands/bitfield_ro
   bitfield_ro,
   /// https://redis.io/commands/bitop
   bitop,
   /// https://redis.io/commands/bitpos
   bitpos,
   /// https://redis.io/commands/blpop
   blpop,
   /// https://redis.io/commands/brpop
   brpop,
   /// https://redis.io/commands/brpoplpush
   brpoplpush,
   /// https://redis.io/commands/bzpopmax
   bzpopmax,
   /// https://redis.io/commands/bzpopmin
   bzpopmin,
   /// https://redis.io/commands/client
   client,
   /// https://redis.io/commands/cluster
   cluster,
   /// https://redis.io/commands/command
   command,
   /// https://redis.io/commands/config
   config,
   /// https://redis.io/commands/dbsize
   dbsize,
   /// https://redis.io/commands/debug
   debug,
   /// https://redis.io/commands/decr
   decr,
   /// https://redis.io/commands/decrby
   decrby,
   /// https://redis.io/commands/del
   del,
   /// https://redis.io/commands/discard
   discard,
   /// https://redis.io/commands/dump
   dump,
   /// https://redis.io/commands/echo
   echo,
   /// https://redis.io/commands/eval
   eval,
   /// https://redis.io/commands/evalsha
   evalsha,
   /// https://redis.io/commands/exec
   exec,
   /// https://redis.io/commands/exists
   exists,
   /// https://redis.io/commands/expire
   expire,
   /// https://redis.io/commands/expireat
   expireat,
   /// https://redis.io/commands/flushall
   flushall,
   /// https://redis.io/commands/flushdb
   flushdb,
   /// https://redis.io/commands/geoadd
   geoadd,
   /// https://redis.io/commands/geodist
   geodist,
   /// https://redis.io/commands/geohash
   geohash,
   /// https://redis.io/commands/geopos
   geopos,
   /// https://redis.io/commands/georadius
   georadius,
   /// https://redis.io/commands/georadius_ro
   georadius_ro,
   /// https://redis.io/commands/georadiusbymember
   georadiusbymember,
   /// https://redis.io/commands/georadiusbymember_ro
   georadiusbymember_ro,
   /// https://redis.io/commands/get
   get,
   /// https://redis.io/commands/getbit
   getbit,
   /// https://redis.io/commands/getrange
   getrange,
   /// https://redis.io/commands/getset
   getset,
   /// https://redis.io/commands/hdel
   hdel,
   /// https://redis.io/commands/hello
   hello,
   /// https://redis.io/commands/hexists
   hexists,
   /// https://redis.io/commands/hget
   hget,
   /// https://redis.io/commands/hgetall
   hgetall,
   /// https://redis.io/commands/hincrby
   hincrby,
   /// https://redis.io/commands/hincrbyfloat
   hincrbyfloat,
   /// https://redis.io/commands/hkeys
   hkeys,
   /// https://redis.io/commands/hlen
   hlen,
   /// https://redis.io/commands/hmget
   hmget,
   /// https://redis.io/commands/hmset
   hmset,
   /// https://redis.io/commands/hscan
   hscan,
   /// https://redis.io/commands/hset
   hset,
   /// https://redis.io/commands/hsetnx
   hsetnx,
   /// https://redis.io/commands/hstrlen
   hstrlen,
   /// https://redis.io/commands/hvals
   hvals,
   /// https://redis.io/commands/incr
   incr,
   /// https://redis.io/commands/incrby
   incrby,
   /// https://redis.io/commands/incrbyfloat
   incrbyfloat,
   /// https://redis.io/commands/info
   info,
   /// https://redis.io/commands/keys
   keys,
   /// https://redis.io/commands/lastsave
   lastsave,
   /// https://redis.io/commands/latency
   latency,
   /// https://redis.io/commands/lindex
   lindex,
   /// https://redis.io/commands/linsert
   linsert,
   /// https://redis.io/commands/llen
   llen,
   /// https://redis.io/commands/lolwut
   lolwut,
   /// https://redis.io/commands/lpop
   lpop,
   /// https://redis.io/commands/lpos
   lpos,
   /// https://redis.io/commands/lpush
   lpush,
   /// https://redis.io/commands/lpushx
   lpushx,
   /// https://redis.io/commands/lrange
   lrange,
   /// https://redis.io/commands/lrem
   lrem,
   /// https://redis.io/commands/lset
   lset,
   /// https://redis.io/commands/ltrim
   ltrim,
   /// https://redis.io/commands/memory
   memory,
   /// https://redis.io/commands/mget
   mget,
   /// https://redis.io/commands/migrate
   migrate,
   /// https://redis.io/commands/module
   module,
   /// https://redis.io/commands/monitor
   monitor,
   /// https://redis.io/commands/move
   move,
   /// https://redis.io/commands/mset
   mset,
   /// https://redis.io/commands/msetnx
   msetnx,
   /// https://redis.io/commands/multi
   multi,
   /// https://redis.io/commands/object
   object,
   /// https://redis.io/commands/persist
   persist,
   /// https://redis.io/commands/pexpire
   pexpire,
   /// https://redis.io/commands/pexpireat
   pexpireat,
   /// https://redis.io/commands/pfadd
   pfadd,
   /// https://redis.io/commands/pfcount
   pfcount,
   /// https://redis.io/commands/pfdebug
   pfdebug,
   /// https://redis.io/commands/pfmerge
   pfmerge,
   /// https://redis.io/commands/pfselftest
   pfselftest,
   /// https://redis.io/commands/ping
   ping,
   /// https://redis.io/commands/post
   post,
   /// https://redis.io/commands/psetex
   psetex,
   /// https://redis.io/commands/psubscribe
   psubscribe,
   /// https://redis.io/commands/psync
   psync,
   /// https://redis.io/commands/pttl
   pttl,
   /// https://redis.io/commands/publish
   publish,
   /// https://redis.io/commands/pubsub
   pubsub,
   /// https://redis.io/commands/punsubscribe
   punsubscribe,
   /// https://redis.io/commands/randomkey
   randomkey,
   /// https://redis.io/commands/readonly
   readonly,
   /// https://redis.io/commands/readwrite
   readwrite,
   /// https://redis.io/commands/rename
   rename,
   /// https://redis.io/commands/renamenx
   renamenx,
   /// https://redis.io/commands/replconf
   replconf,
   /// https://redis.io/commands/replicaof
   replicaof,
   /// https://redis.io/commands/restore
   restore,
   /// https://redis.io/commands/role
   role,
   /// https://redis.io/commands/rpop
   rpop,
   /// https://redis.io/commands/rpoplpush
   rpoplpush,
   /// https://redis.io/commands/rpush
   rpush,
   /// https://redis.io/commands/rpushx
   rpushx,
   /// https://redis.io/commands/sadd
   sadd,
   /// https://redis.io/commands/save
   save,
   /// https://redis.io/commands/scan
   scan,
   /// https://redis.io/commands/scard
   scard,
   /// https://redis.io/commands/script
   script,
   /// https://redis.io/commands/sdiff
   sdiff,
   /// https://redis.io/commands/sdiffstore
   sdiffstore,
   /// https://redis.io/commands/select
   select,
   /// https://redis.io/commands/set
   set,
   /// https://redis.io/commands/setbit
   setbit,
   /// https://redis.io/commands/setex
   setex,
   /// https://redis.io/commands/setnx
   setnx,
   /// https://redis.io/commands/setrange
   setrange,
   /// https://redis.io/commands/shutdown
   shutdown,
   /// https://redis.io/commands/sinter
   sinter,
   /// https://redis.io/commands/sinterstore
   sinterstore,
   /// https://redis.io/commands/sismember
   sismember,
   /// https://redis.io/commands/slaveof
   slaveof,
   /// https://redis.io/commands/slowlog
   slowlog,
   /// https://redis.io/commands/smembers
   smembers,
   /// https://redis.io/commands/smove
   smove,
   /// https://redis.io/commands/sort
   sort,
   /// https://redis.io/commands/spop
   spop,
   /// https://redis.io/commands/srandmember
   srandmember,
   /// https://redis.io/commands/srem
   srem,
   /// https://redis.io/commands/sscan
   sscan,
   /// https://redis.io/commands/stralgo
   stralgo,
   /// https://redis.io/commands/strlen
   strlen,
   /// https://redis.io/commands/subscribe
   subscribe,
   /// https://redis.io/commands/substr
   substr,
   /// https://redis.io/commands/sunion
   sunion,
   /// https://redis.io/commands/sunionstore
   sunionstore,
   /// https://redis.io/commands/swapdb
   swapdb,
   /// https://redis.io/commands/sync
   sync,
   /// https://redis.io/commands/time
   time,
   /// https://redis.io/commands/touch
   touch,
   /// https://redis.io/commands/ttl
   ttl,
   /// https://redis.io/commands/type
   type,
   /// https://redis.io/commands/unlink
   unlink,
   /// https://redis.io/commands/quit
   quit,
   /// https://redis.io/commands/unsubscribe
   unsubscribe,
   /// https://redis.io/commands/unwatch
   unwatch,
   /// https://redis.io/commands/wait
   wait,
   /// https://redis.io/commands/watch
   watch,
   /// https://redis.io/commands/xack
   xack,
   /// https://redis.io/commands/xadd
   xadd,
   /// https://redis.io/commands/xclaim
   xclaim,
   /// https://redis.io/commands/xdel
   xdel,
   /// https://redis.io/commands/xgroup
   xgroup,
   /// https://redis.io/commands/xinfo
   xinfo,
   /// https://redis.io/commands/xlen
   xlen,
   /// https://redis.io/commands/xpending
   xpending,
   /// https://redis.io/commands/xrange
   xrange,
   /// https://redis.io/commands/xread
   xread,
   /// https://redis.io/commands/xreadgroup
   xreadgroup,
   /// https://redis.io/commands/xrevrange
   xrevrange,
   /// https://redis.io/commands/xsetid
   xsetid,
   /// https://redis.io/commands/xtrim
   xtrim,
   /// https://redis.io/commands/zadd
   zadd,
   /// https://redis.io/commands/zcard
   zcard,
   /// https://redis.io/commands/zcount
   zcount,
   /// https://redis.io/commands/zincrby
   zincrby,
   /// https://redis.io/commands/zinterstore
   zinterstore,
   /// https://redis.io/commands/zlexcount
   zlexcount,
   /// https://redis.io/commands/zpopmax
   zpopmax,
   /// https://redis.io/commands/zpopmin
   zpopmin,
   /// https://redis.io/commands/zrange
   zrange,
   /// https://redis.io/commands/zrangebylex
   zrangebylex,
   /// https://redis.io/commands/zrangebyscore
   zrangebyscore,
   /// https://redis.io/commands/zrank
   zrank,
   /// https://redis.io/commands/zrem
   zrem,
   /// https://redis.io/commands/zremrangebylex
   zremrangebylex,
   /// https://redis.io/commands/zremrangebyrank
   zremrangebyrank,
   /// https://redis.io/commands/zremrangebyscore
   zremrangebyscore,
   /// https://redis.io/commands/zrevrange
   zrevrange,
   /// https://redis.io/commands/zrevrangebylex
   zrevrangebylex,
   /// https://redis.io/commands/zrevrangebyscore
   zrevrangebyscore,
   /// https://redis.io/commands/zrevrank
   zrevrank,
   /// https://redis.io/commands/zscan
   zscan,
   /// https://redis.io/commands/zscore
   zscore,
   /// https://redis.io/commands/zunionstore
   zunionstore,
   /// Invalid command.
   invalid
};

/** \brief Converts a command to a string
 *  \ingroup any
 *
 *  \param c The command to convert.
 */
char const* to_string(command c);

/** \brief Write the text for a command name to an output stream.
 *  \ingroup operators
 *
 *  \param os Output stream.
 *  \param c Redis command
 */
std::ostream& operator<<(std::ostream& os, command c);

/** \brief Returns true for commands with push response.
 *  \ingroup any
 */
bool has_push_response(command cmd);

} // redis
} // aedis

#endif // AEDIS_REDIS_COMMAND_HPP
