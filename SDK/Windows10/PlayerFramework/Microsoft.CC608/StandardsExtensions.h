
// these Standard extensions are expected to be included in the next C++ standard after C++11--according to Herb Sutter (Chair of the Standards Committee), their
// omission from C++11 was an oversight.

namespace stdExt {

// constant non-member begin() 
template<class T>
auto cbegin(const T& t) -> decltype( t.cbegin() ) { return t.cbegin(); }

// constant non-member end()
template<class T>
auto cend(const T& t) -> decltype (t.cend() ) { return t.cend(); }

}