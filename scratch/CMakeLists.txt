# coronet - An experimental networking library that supports both the
#           Universal Model of the Networking TS and the coroutines of
#           the Coroutines TS.
#
#  Copyright Eric Niebler 2017
#
#  Use, modification and distribution is subject to the
#  Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at
#  http:#www.boost.org/LICENSE_1_0.txt)
#
# Project home: https://github.com/ericniebler/coronet

add_executable(scratch scratch.cpp)
target_link_libraries(scratch coronet)
