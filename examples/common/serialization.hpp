/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#define BOOST_JSON_NO_LIB
#define BOOST_CONTAINER_NO_LIB
#include <boost/redis.hpp>
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <boost/json.hpp>

using namespace boost::describe;
using namespace boost::json;

template <
      class T,
      class Bd = describe_bases<T, mod_any_access>,
      class Md = describe_members<T, mod_any_access>,
      class En = std::enable_if_t<!std::is_union<T>::value>
   >
std::ostream& operator<<(std::ostream& os, T const & t)
{
    os << "{";

    bool first = true;

    boost::mp11::mp_for_each<Bd>([&](auto D){

        if( !first ) { os << ", "; } first = false;

        using B = typename decltype(D)::type;
        os << (B const&)t;

    });

    boost::mp11::mp_for_each<Md>([&](auto D){

        if( !first ) { os << ", "; } first = false;

        os << "." << D.name << " = " << t.*D.pointer;

    });

    os << "}";
    return os;
}

template <
      class T,
      class D1 = boost::describe::describe_members<T, boost::describe::mod_public | boost::describe::mod_protected>,
      class D2 = boost::describe::describe_members<T, boost::describe::mod_private>,
      class En = std::enable_if_t<boost::mp11::mp_empty<D2>::value && !std::is_union<T>::value>
   >
void tag_invoke(boost::json::value_from_tag const&, boost::json::value& v, T const & t)
{
    auto& obj = v.emplace_object();

    boost::mp11::mp_for_each<D1>([&](auto D){

        obj[ D.name ] = boost::json::value_from( t.*D.pointer );

    });
}

template<class T> void extract( boost::json::object const & obj, char const * name, T & value )
{
    value = boost::json::value_to<T>( obj.at( name ) );
}

template<class T,
    class D1 = boost::describe::describe_members<T,
        boost::describe::mod_public | boost::describe::mod_protected>,
    class D2 = boost::describe::describe_members<T, boost::describe::mod_private>,
    class En = std::enable_if_t<boost::mp11::mp_empty<D2>::value && !std::is_union<T>::value> >

    T tag_invoke( boost::json::value_to_tag<T> const&, boost::json::value const& v )
{
    auto const& obj = v.as_object();

    T t{};

    boost::mp11::mp_for_each<D1>([&](auto D){

        extract( obj, D.name, t.*D.pointer );

    });

    return t;
}

// Serialization
template <class T>
void boost_redis_to_bulk(std::string& to, T const& u)
{
   boost::redis::resp3::boost_redis_to_bulk(to, serialize(value_from(u)));
}

template <class T>
void boost_redis_from_bulk(T& u, std::string_view sv, boost::system::error_code&)
{
   value jv = parse(sv);
   u = value_to<T>(jv);
}
