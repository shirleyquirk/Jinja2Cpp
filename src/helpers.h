#ifndef JINJA2CPP_SRC_HELPERS_H
#define JINJA2CPP_SRC_HELPERS_H

#include <string_view>
#include <string>
#include <type_traits>

namespace jinja2
{
#define UNIVERSAL_STR(Str) Str
//! CompileEscapes replaces escape characters by their meanings.
/**
 * @param[in] s Characters sequence with zero or more escape characters.
 * @return Characters sequence copy where replaced all escape characters by
 * their meanings.
 */
template<typename Sequence>
Sequence CompileEscapes(Sequence s)
{
   auto itr1 = s.begin();
   auto itr2 = s.begin();
   const auto end = s.cend();

   auto removalCount  = 0;

   while (end != itr1)
   {
      if ('\\' == *itr1)
      {
         ++removalCount;

         if (end == ++itr1)
            break;
         if ('\\' != *itr1)
         {
            switch (*itr1)
            {
               case 'n': *itr1 = '\n'; break;
               case 'r': *itr1 = '\r'; break;
               case 't': *itr1 = '\t'; break;
               default:                break;
            }

            continue;
         }
      }

      if (itr1 != itr2)
         *itr2 = *itr1;

      ++itr1;
      ++itr2;
   }

   s.resize(s.size() - removalCount);

   return s;
}

} // namespace jinja2

#endif // JINJA2CPP_SRC_HELPERS_H
