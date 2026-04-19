                #pragma once

                // simple example in online compiler for x86-64 (32bit), where output
                // can be viewed in the online compiler-
                // https://godbolt.org/z/a1x1934on
                // you can experiment with the usage in the online compiler

                #include <stdint.h> //avr does not have <cstdint>

//========================================================================================
namespace
FMT             
//========================================================================================
                {
                
                //we may already have these in the global namespace
                //but if not we will create them here for our use in the FMT namespace
                //(duplicates are ok if the type names refer to the same types)

                using u64 = uint64_t; using i64 = int64_t; 
                using u32 = uint32_t; using i32 = int32_t; 
                using u16 = uint16_t; using i16 = int16_t; 
                using u8  = uint8_t;  using i8  = int8_t; 

                
                // uintN_t/intN_t coverage
                //                                avr       cortex-m   x64-86 32bit
                // unsigned char                 uint8_t     uint8_t     uint8_t
                // signed char                    int8_t      int8_t      int8_t
                // unsigned short               uint16_t    uint16_t    uint16_t
                // [signed] short                int16_t     int16_t     int16_t
                // unsigned int                 uint16_t      ---       uint32_t
                // [signed] int                  int16_t      ---        int32_t
                // unsigned long [int]          uint32_t    uint32_t      --- 
                // [signed] long [int]           int32_t     int32_t      ---
                // unsigned long long [int]     uint64_t    uint64_t    uint64_t
                // [signed] long long [int]      int64_t     int64_t     int64_t
                // literals default              int16_t      ---        int32_t
                // literals U                   uint16_t      ---       uint32_t
                // literals L                    int32_t     int32_t      ---
                // literals UL                  uint32_t    uint32_t      ---
                // literals LL                   int64_t     int64_t     int64_t
                // literals ULL                 uint64_t    uint64_t    uint64_t

                // to cover all types, we will take advantage of the fact the
                // long type is always 32 bits for any target which is 32 bit
                // or lower (so for example, x64-86 in 64bit mode would be excluded)
                // and the fact that all the integral types end up in the 32 bit
                // print overload
                // 
                // the print function will be overloaded with the built-in language
                // types where they can differ-
                // unsigned short / short
                // unsigned int / int
                // unsigned long / long <-- these are the destinations of the previous types
                //
                // the print overload types of the first two lines will cast to the 
                // print overload of the long or unsigned long type (or u32/i32), so all types
                // will be covered and we do not have to be concerned about whether an int is 
                // 16 or 32 bits,  or whether a long is the same size as an int (they are 
                // different types even if the same size)

                //enum values which will be used to choose a particular overloaded
                // print function, where the enum value is either the only value
                // needed (2 or more enums) or only needed to choose an overloaded
                // function but not otherwise used (single value enums)
                enum JUSTIFY { //if width padding, justify type
right,              // default
left,
internal        };
                enum BASE { //base numbering
bin,
oct,
dec,                // default
hex             };
                enum SHOWBASE { 
noshowbase,         // default 0A (hex) 00001010 (bin) 012 (oct)
showbase        };  // 0x0A (hex) 0b00001010 (bin) 0o12 (oct)
                enum UPPERCASE { //hex uppercase or lowercase
nouppercase,        // default 0x0a
uppercase       };  // 0x0A
                enum BOOLALPHA { //binary output type
noboolalpha,        // default bin 1|0                
boolalpha       };  // bin "true"|"false"
                enum SHOWPOS { //decimal  positive 
noshowpos,          // default 0 1 2
showpos         };  // 0 +1 +2
                enum RESET { 
reset           };  // all options reset
                enum ENDL { 
endl            };  // newline                
                enum ENDL2 { 
endl2           }; // endl x2
                enum COUNTCLR { 
countclr        }; //clear char out counter


//........................................................................................
class           //FMT::
Print           
//........................................................................................
                {

                //no 64bit support

                //max float precision, limited to 9 by use of 32bit integers in calculations
                static constexpr u8 PRECISION_MAX{ 9 };                

public:

                //print a string, size is specified
                Print&
print           (const char* str, u16 n)
                {
                int pad = width_ - n;                           //will pad with positive results
                width_ = 0;                                     //always reset width_ after use
                isNeg_ = false;                                 //clear for the other 2 functions (since they both will end up here)
                auto wr = [&]{ while(n--) write_( *str++ ); };  //lambda to write str
                if( pad <= 0 or just_ == left ) wr();           //print str first
                if( pad > 0 ){                                  //padding needed?
                    while( pad-- > 0 ) write_( fill_ );         //print any needed padding
                    if( just_ == right ) wr();                  //and print str if was not done already
                    }
                return *this;
                }
                
                //string without size info, make usable for above function
                Print&
print           (const char* str){ return print( str, __builtin_strlen(str) ); }

                //prints u32 as string
                Print&
print           (const unsigned long v)
                {
                static constexpr char charTableUC[]{ "0123456789ABCDEF" };
                static constexpr char charTableLC[]{ "0123456789abcdef" };
                static constexpr u8 BUFSZ{ 32+2 };              //0bx...x-> 32+2 digits max (no 0 termination needed as we use str+strsiz)
                char buf[BUFSZ];                                //will be used as a string, with no 0 termination
                u8 idx = BUFSZ;                                 //start past end, so pre-decrement bufidx
                const char* ctbl{ uppercase_ ? charTableUC : charTableLC }; //use uppercase or lowercase char table
                auto u = v;                                     //make copy to use (v is const)
                auto w = just_ == internal ? width_ : 0;        //use width_ here if internal justify
                if( w ){
                    width_ = 0;                                 //if internal, clear width_ so not used when value printed
                    just_ = right;                              //reset just_ if internal (so no need to reset in other code)
                    }                                           //(don't see where it would ever be useful to leave as internal)

                //lambda function to insert char to buf (idx decrementing)
                //buffer insert in reverse order (high bytes to low bytes)
                //(decrement w unless is 0, w is for internal width)
                auto insert = [&](char c){ buf[--idx] = c; if(w) --w; }; 

                //optimize for each base as mcu may not have hardware division
                switch(base_){
                    //all do at least once (u can be 0, so need 1 output char before 0 test)
                    case bin: //ascii hex value- 0,1
                        do{ insert( ctbl[u bitand 1] ); } while( u >>= 1, u );
                        break;
                    case oct: //ascii hex value- 0-7
                        do{ insert( ctbl[u bitand 7] ); } while( u >>= 3, u );
                        break;
                    case hex: //ascii hex value- 0-F
                        do{ insert( ctbl[u bitand 15] ); } while( u >>= 4, u );
                        break;
                    case dec: //ascii hex value- 0-9
                        do{ insert( ctbl[u % 10] ); } while( u /= 10, u );
                        break;
                    }

                //internal justify width could overflow buf, so limit width to allow space for 
                //2 more chars in the buffer (0b,0x could be added below)
                while( w and (idx > 2) ){ insert( fill_ ); } //internal fill

                switch( base_ ){
                    case bin: if(showbase_)         { insert('b'); insert('0'); }   break; //0b
                    //oct -  0 -> 0  or 0o0   27 -> 027 or 0o27
                    case oct: if(showbase_)         { insert('o'); }                       //o (for 0o)
                              if(showbase_ or v)    { insert('0'); }                break; //0 (for 0o and for 01)
                    case hex: if(showbase_)         { insert('x'); insert('0'); }   break; //0x
                    case dec: if(isNeg_)            { insert('-'); break; }                       //- if negative (no need to check if 0)
                              if(pos_ and v )       { insert('+'); }                break; //+ if pos_ set and >0
                    }
                return print( &buf[idx], BUFSZ-idx );
                }

                //double, prints integer part and decimal part separately as integers
                Print&
print           (const double cd)
                {
                static const char* errs[]{ "nan", "inf", "ovf" };
                const char* perr{ nullptr };
                //check for nan/inf
                if( __builtin_isnan(cd) ) perr = errs[0];
                else if( __builtin_isinf_sign(cd) ) perr = errs[1];
                //we are limited by choice to using 32bit integers, so check for limits we can handle
                //raw float value of 0x4F7FFFFF is 4294967040.0, and the next value we will exceed a 32bit integer
                else if( cd > 4294967040.0 or (cd < -4294967040.0) ) perr = errs[2];
                if( perr ){
                    width_ = 0;
                    return print( perr ); 
                    }

                isNeg_ = cd < 0;                // set isNeg_ for first integer print
                double d { isNeg_ ? -cd : cd }; // [d]ouble, copy cd, any negative value to positive
                u32 di { static_cast<u32>(d) }; // [d]ouble_as_[i]nteger, integer part to print
                double dd { d - di };           // [d]ouble[d]ecimal part
                u32 i09 { 0 };                  // current integer digit of decimal part x10
                auto pre { precision_ };        // a copy to decrement 
                u32 ddi { 0 };                  // [d]ouble[d]ecimal_as[i]nteger, conversion of decimal part to integer

                //first print integer part, uses pos_/isNeg_/width_/just_
                //second decimal part (also imteger) needs pos_=noshowbase,just_=right,width_=precision_,fill_='0'
                //width_ and isNeg_ reset on each use, so no need to save/restore
                auto base_save{ base_ };    base_ = dec;
                auto justify_save{ just_ }; just_ = right;
                auto pos_save = pos_;
                auto save_fill = fill_;

                //handle decimal part of float
                while( pre-- ){                 //loop precision_ times
                    dd *= 10;                   //shift each significant decimal digit into integer
                    i09 = static_cast<u32>(dd); //integer 0-9
                    ddi = ddi*10 + i09;         //shift in new integer to integer-decimal print value
                    dd -= i09;                  //and remove the 0-9 integer (so only decimal remains)
                    }

                //apply rounding rules  
                //  round up/nearest if >0.5 remains
                //  round to nearest even value if 0.5 remains
                if( dd > 0.5 ){
                    if( precision_ ) ddi++;     //inc integer-decimal part
                    else di++ ;                 //inc integer part if no decimals in use
                    }
                else if( dd == 0.5 ){
                    if( precision_ ) ddi += i09 & 1;//make integer-decimal even based on last digit
                    else di += di & 1;           //same, but on di since no decimals in use
                    }

                print( di );                   //print integer part of float

                //if precision is not 0, print the decimal part of the float
                if( precision_ ){
                    print( '.' ); 
                    pos_ = noshowpos;           //no +
                    fill_ = '0';                //0 pad
                    width_ = precision_;        //width same as precision (width_ always resets to 0 on each use)
                    print( ddi );               //print decimal part as integer
                    }

                base_ = base_save;              //restore base_            
                just_ = justify_save;           //restore just_
                pos_ = pos_save;                //restore pos_
                fill_ = save_fill;              //restore fill_   
                return *this;
                }

                //reset all options to default (except newline), clear count
                Print&
print           (RESET)
                {
                just_ = right;
                showbase_ = noshowbase;
                uppercase_ = nouppercase;
                boolalpha_ = noboolalpha;
                pos_ = noshowpos;
                isNeg_ = false;
                count_ = 0;
                width_ = 0;
                fill_ = ' ';
                base_ = dec;
                precision_ = PRECISION_MAX;
                return *this;
                }

                //all other printing overloaded print functions, integers will end up
                //in the print(u32) function above 
                
                //(we cast any 64bit numbers that will fit in u32/i32, otherwise we print !u64! or !i64! 
                //to note the number is unhandled (note- these types will always be long long types for any of our targets)
                Print& print     (const uint64_t n) { return n bitand 0xFFFFFFFF00000000 ? print("!u64!") : print( static_cast<u32>(n) ); }
                Print& print     (const int64_t n)  { return n bitand 0xFFFFFFFF00000000 ? print("!i64!"): print( static_cast<i32>(n) ); }
                
                //using standard types when they can differ, so all types are handled in all targets
                //a long type is always 32 bits, and we will cast all other types up to a long type (or u32/i32)
                //(ultimately all end up in the print(unsigned long) overload)
                Print& print    (const long n)          { unsigned long nu = n; if( n < 0 ){ isNeg_ = true; nu = -nu; } return print( nu ); }      
                Print& print    (const int n)           { return print( static_cast<long>(n) ); }
                Print& print    (const unsigned n)      { return print( static_cast<unsigned long>(n) ); }
                Print& print    (const short n)         { return print( static_cast<i32>(n) ); }
                Print& print    (const unsigned short n){ return print( static_cast<u32>(n) ); }
                Print& print    (const i8 n)            { return print( static_cast<i32>(n) ); }
                Print& print    (const u8 n)            { return print( static_cast<u32>(n) ); }

                Print& print    (const char c)      { write_( c ); width_ = 0; return *this; } //direct output, no conversion
                Print& print    (const bool tf)     { return boolalpha_ ? print( tf ? "true" : "false" ) : print( tf ? '1' : '0'); }                
                Print& print    (void* const n)     { return print( reinterpret_cast<u32>(n) ); }

                //non-printing overloaded print functions deduced via enum
                Print& print    (BASE e)            { base_ = e; return *this;}
                Print& print    (SHOWBASE e)        { showbase_ = e; return *this;}
                Print& print    (UPPERCASE e)       { uppercase_ = e; return *this;}
                Print& print    (BOOLALPHA e)       { boolalpha_ = e; return *this;}
                Print& print    (SHOWPOS e)         { pos_ = e; return *this;}
                Print& print    (JUSTIFY e)         { just_ = e; return *this;}
                Print& print    (ENDL)              { return nl_[1] ? print( nl_ ) : print( nl_[0] ); } //if 1 char, just print char. else as string
                Print& print    (ENDL2)             { return print(endl), print(endl); }
                Print& print    (COUNTCLR)          { count_ = 0; return *this;}

                //non-printing functions needing a non-enum value, typically called by operator<< code
                Print& width    (const u16 v)       { width_ = v; return *this; } //setw
                Print& fill     (const char v)      { fill_ = v; return *this; } //setfill
                Print& precision(const u8 v)        { precision_ = v > PRECISION_MAX ? PRECISION_MAX : v; return *this; } //setprecision

                //setup newline char(s) (up to 2)
                Print& newline  (const char* str)   { nl_[0] = str[0]; nl_[1] = str[1]; return *this; } //setnewline

                //read some private vars, add as needed
                auto   count    () const            { return count_; }
                auto   base     () const            { return base_; }
                auto   justify  () const            { return just_; }
                auto   width    () const            { return width_; }
                auto   fill     () const            { return fill_; }

private:
                //a helper write so we can keep a count of chars written (successfully)
                //(if any write fails as defined by the parent class (returns false), the failure
                // is only reflected in the count and not used any further)
                void   write_   (const char c)      { if( write(c) and (count_ < decltype(count_)(-1) ) ) ++count_; }

                //parent class creates this function
                virtual
                bool   write    (const char) = 0;

                char            nl_[3]      { '\n', '\0', '\0' }; //[2] never changes
                JUSTIFY         just_       { left };
                SHOWBASE        showbase_   { noshowbase };
                UPPERCASE       uppercase_  { nouppercase };
                BOOLALPHA       boolalpha_  { noboolalpha };
                SHOWPOS         pos_        { noshowpos };
                bool            isNeg_      { false };  //inform other functions a number was originally negative
                u8              width_      { 0 }; //limit to 255 (see no need for large numbers in padding)
                char            fill_       { ' ' };
                u16             count_      { 0 };
                u8              precision_  { PRECISION_MAX };
                BASE            base_       { dec };

                }; //class FMT::Print



//........................................................................................
                //sprintf-like, where we can use the Print class to format a char buffer
                //whose max size is set by the template parameter N

                template<u16 N>
class           //FMT::
Sprint          : public Print     
//........................................................................................
                {

                //print to an array of specified size N 
                //(we reserved 1 byte for the terimating \0)
                char arr_[N+1];
                u16 idx_;

                virtual bool
write           (const char c)
                {
                if( idx_ >= N ) return false;
                arr_[idx_++] = c; 
                arr_[idx_] = 0; //always 0 terminate as we go
                return true;
                }

public:

Sprint          (){ clear(); }

                //0 terminate, reset idx
                Sprint&
clear           (){ arr_[0] = 0; idx_ = 0; return *this; }

                //allow outside access to arr_, const 'should' prevent others writing
                const char*
c_str           (){ return arr_; }

                //size used, not including terminating \0
                auto
size            (){ return idx_; }
                auto
length          (){ return idx_; }

                auto
empty           (){ return idx_ == 0; }

                auto
capacity        (){ return N; }                

                }; //class FMT::Sprint



//........................................................................................

                //FMT::PrintNull class (no  output, optimizes away all uses)
                //inherit this instead of Print to make all uses optimize away
                //class Uart : FMT::Print {} //prints
                //class Uart : FMT::PrintNull {} //any use of << will be optimized away

//........................................................................................
class           //FMT::
PrintNull       {}; //just a class type, nothing inside
//........................................................................................

                // operator <<

                //for PrintNull- do nothing, and
                // return the same PrintNull reference passed in
                // (nothing done, no code produced when optimized)
                template<typename T> inline PrintNull& //anything passed in
operator <<     (PrintNull& p, const T){ return p; }

                template<typename T> inline PrintNull& //anything passed in
operator ,      (PrintNull& p, const T){ return p; }




//........................................................................................

                //for Print-
                //use structs or enums to contain a value or values and make it unique
                // so these values can be distinguished from values to print,
                // structs also allows more arguments to be passed into operator <<

                //everything that doesn't match any other function template will be
                //passed to Print::print() and the Print class can sort it out
                template<typename T> inline Print&
                operator << (Print& p, const T t) { return p.print( t ); }

                //also allow comma usage-
                // uart, "123", setwf(10,' '), 456, endl;
                //simply 'convert' all , usage to << usage
                template<typename T> inline Print&
                operator , (Print& p, const T t) { return p << t; }

                //the functions which require a value will return
                //a struct with a value or values which then gets used by <<
                //all functions in the Print class called from these functions are public

                // << setw(10)
                //width is always reset to 0 after each use
                struct Setw { const u8 n; }; //a specific type
                inline Setw //this function returns a Setw struct with values
setw            (const u8 n) { return {n}; } //u8 v -> Setw { v }
                inline Print& //return the print reference
                operator<< (Print& p, Setw s) { return p.width(s.n); } //operator<< for Setw

                // << setfill(' ')
                struct Setf { const char c; };
                inline Setf
setfill         (const char c) { return {c}; }
                inline Print&
                operator<< (Print& p, Setf s) { return p.fill(s.c); }

                // << setprecision(4)
                struct Setp { const u8 n; };
                inline Setp
setprecision    (const u8 n) { return {n}; }
                inline Print&
                operator<< (Print& p, Setp s) { return p.precision(s.n); }

                // << setwf(8,'0')
                struct Setwf { const u8 n; const char c; };
                inline Setwf
setwf           (const u8 n, const char c) { return {n,c}; }
                inline Print&
                operator<< (Print& p, Setwf s) { return p << setw(s.n) << setfill(s.c); }

                // << cdup('=', 40)
                struct Setdup { const char c; const u16 n; };
                inline Setdup
cdup            (const char c, const u16 n) { return {c,n}; }
                inline Print&
                operator<< (Print& p, Setdup s) { return p << setwf(s.n,s.c) << ""; }


                // generic padding helper-
                // PadBase -->  n = total width, v = value, c = padding char, b = base

                template<typename T> struct PadBase { const u8 n; const T v; const char c; FMT::BASE b; };
                template<typename T> inline PadBase<const T> 
padBase_        (const u8 n, const T v, const char c, FMT::BASE b) { return {n,v,c,b}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadBase<const T> s) { return p << nouppercase << s.b << internal << setwf(s.n,s.c) << s.v; }
                template<> inline Print& //type is char (we need to cast to u8 so is treated as a value instead of direct output char)
                operator<< (Print& p, PadBase<const char> s) { return p << padBase_( s.n, static_cast<u8>(s.v), s.c, s.b ); }

                //now using the above helper padBase_ for hex/dec/bin functions

                // << hex_(8,0x1a)  -->>       1a
                // (hex_  -  the _ designates space padding)
                template<typename T> struct Padh_ { const u8 n; const T v; };
                template<typename T> inline Padh_<const T> 
hex_            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, Padh_<const T> s) { return p << nouppercase << noshowbase << padBase_(s.n, s.v, ' ', hex); }

                // << hex0(8,0x1a)  -->> 0000001a
                // (hex0  - the 0 designates 0 padding)
                template<typename T> struct Padh0 { const u8 n; const T v; };
                template<typename T> inline Padh0<const T> 
hex0            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, Padh0<const T> s) { return p << nouppercase << noshowbase << padBase_(s.n, s.v, '0', hex); }

                // << Hex0(8,0x1a)  -->> 0000001A
                // (Hex0  - the uppercase H designates uppercase, the 0 designates 0 padding)
                template<typename T> struct PadH0 { const u8 n; const T v; };
                template<typename T> inline PadH0<const T> 
Hex0            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadH0<const T> s) { return p << uppercase << noshowbase << padBase_(s.n, s.v, '0', hex); }

                // << hex0x(8,0x1a)  -->> 0x0000001a
                // (hex0x  - the 0 designates 0 padding, the x designates a leading 0x)
                template<typename T> struct Padh0x { const u8 n; const T v; };
                template<typename T> inline Padh0x<const T> 
hex0x           (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, Padh0x<const T> s) { return p << nouppercase << showbase << padBase_( s.n, s.v, '0', hex ); }

                // << Hex0x(8,0x1a) -->> 0x0000001A
                // (Hex0x  - the H designates uppercase, the 0 designates 0 padding, the x designates a leading 0x)
                template<typename T> struct PadH0x { const u8 n; const T v; };
                template<typename T> inline PadH0x<const T> 
Hex0x           (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadH0x<const T> s) { return p << uppercase << showbase << padBase_( s.n, s.v, '0', hex ); }

                // << dec_(8,123)  -->>      123
                // (dec_  -  the _ designates space padding)
                template<typename T> struct PadD_ { const u8 n; const T v; };
                template<typename T> inline PadD_<const T> 
dec_            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadD_<const T> s) { return p << padBase_( s.n, s.v, ' ', dec ); }

                // << dec0(8,123)  -->> 00000123
                // (dec0  -  the 0 designates 0 padding)
                template<typename T> struct PadD0 { const u8 n; const T v; };
                template<typename T> inline PadD0<const T> 
dec0            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadD0<const T> s) { return p << padBase_( s.n, s.v, '0', dec ); }

                // << bin_(8,123)  -->>  1111011
                // (bin_  -  the _ designates space padding)
                template<typename T> struct PadB_ { const u8 n; const T v; };
                template<typename T> inline PadB_<const T> 
bin_            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadB_<const T> s) { return p << noshowbase << padBase_( s.n, s.v, ' ', bin ); }

                // << bin0(8,123)  -->> 01111011
                // (bin0  -  the 0 designates 0 padding)
                template<typename T> struct PadB0 { const u8 n; const T v; };
                template<typename T> inline PadB0<const T> 
bin0            (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadB0<const T> s) { return p << noshowbase << padBase_( s.n, s.v, '0', bin ); }

                // << bin0b(8,123)  -->> 0b01111011
                // (bin0b  - the 0 designates 0 padding, the b designates a leading 0b)
                template<typename T> struct PadB0b { const u8 n; const T v; };
                template<typename T> inline PadB0b<const T> 
bin0b           (const u8 n, const T v) { return {n,v}; }
                template<typename T> inline Print&
                operator<< (Print& p, PadB0b<const T> s) { return p << showbase << padBase_( s.n, s.v, '0', bin ); }

                // << strHex("123")  -->> 313233
                // string to hex values
                // optional braces, 2 chars as string, \0\0 is default, \0 = no brace for postition
                //      strHex("123","[]")      -->> [313233]
                //      strHex("123","_")       -->> _313233 
                //      strHex("123","\0_")     -->> 313233_ 
                struct StrHex { const char* str; const char* braces; };
                inline StrHex
strToHex        (const char* str, const char* braces = "\0\0"){ return {str,braces}; }
                inline Print&
                operator << (Print& p, StrHex s){
                    if( s.braces[0] ) p << s.braces[0];
                    for( ; *s.str; s.str++ ){
                        p << Hex0(2,*s.str);
                        }
                    return s.braces[1] ? p << s.braces[1] : p;
                    }

                // << space -->> ' '
                enum Space { 
space           };
                inline Print&
                operator<< (Print& p, Space) { return p << ' '; }

                // << spaces(5) -->> "     "
                struct Spaces { const u8 n; };
                inline Spaces
spaces          (const u8 n){ return {n}; }
                inline Print&
                operator<< (Print& p, Spaces s) { return p << cdup(' ',s.n); }

                // << tab -->> '\t'
                enum Tab { 
tab             };
                inline Print&
                operator<< (Print& p, Tab) { return p << '\t'; }

                // << tabs(2) -->> "\t\t"
                struct Tabs { const u8 n; };
                inline Tabs
tabs            (const u8 n){ return {n}; }
                inline Print&
                operator<< (Print& p, Tabs s) { return p << cdup('\t',s.n); }



//........................................................................................
namespace       //FMT::
ANSI  
//........................................................................................
                {

                //ansi_ a runtime var, so ansi output can be enabled/disabled
                //at runtime
                inline auto ansi_{ true }; //default is enabled

                //set on (true) or off (false)
                inline auto
ansi            (bool tf){ ansi_ = tf; }  
                inline auto
ansiOn          (){ ansi(true); }  
                inline auto
ansiOff         (){ ansi(false); }  
                //get status
                inline auto
ansi            (){ return ansi_; }


                //the functions which require a value will return
                //a struct with a value or values which then gets used by <<
                //all functions called in Print are public
                //enums also used to 'find' the specific operator<< when no values are needed

                //ansi rgb colors (used by fg,bg)
                //init with u8's or a u32 (lower 24bits - xxRRGGBB)
                struct Rgb { 
                    const u8 r; const u8 g; const u8 b; 
                    constexpr Rgb(const u8 r_,const u8 g_,const u8 b_)
                        : r{ r_ }, g{ g_ }, b{ b_ }
                          {} 
                    constexpr Rgb(const u32 rgb)
                        : Rgb{ static_cast<u8>(rgb>>16),
                               static_cast<u8>(rgb>>8),
                               static_cast<u8>(rgb) }
                          {} 
                    };

                //ansi fg(Rgb) or fg( Rgb{r,g,b} )
                struct Fg { const Rgb rgb; };
                inline Fg 
fg              (const Rgb s) { return {s}; }
                inline Print& 
                operator<< (Print& p, Fg s) { return ansi_ ?  p << "\033[38;2;" << dec << s.rgb.r << ';' << s.rgb.g << ';' << s.rgb.b << 'm' : p; }

                //ansi bg(Rgb) or bg( Rgb{r,g,b} )
                struct Bg { const Rgb rgb; };
                inline Bg 
bg              (const Rgb s) { return {s}; }
                inline Print&
                operator<< (Print& p, Bg s) { return ansi_ ? p << "\033[48;2;" << dec << s.rgb.r << ';' << s.rgb.g << ';' << s.rgb.b << 'm' : p; }

                //ansi cls
                enum CLS { 
cls             };
                inline Print&
                operator<< (Print& p, CLS) { return ansi_ ? p << "\033[2J" : p; }

                //ansi home
                enum HOME { 
home            };
                inline Print&
                operator<< (Print& p, HOME) { return ansi_ ? p << "\033[1;1H" : p; }

                //ansi italic
                enum ITALIC { 
italic          };
                inline Print&
                operator<< (Print& p, ITALIC) { return ansi_ ? p << "\033[3m" : p; }

                //ansi normal
                enum NORMAL { 
normal          };
                inline Print&
                operator<< (Print& p, NORMAL) { return ansi_ ? p << "\033[0m" : p; }

                //ansi normal
                enum BOLD { 
bold            };
                inline Print&
                operator<< (Print& p, BOLD) { return ansi_ ? p << "\033[1m" : p; }

                //ansi underline
                enum UNDERLINE { 
underline       };
                inline Print&
                operator<< (Print& p, UNDERLINE) { return ansi_ ? p << "\033[4m" : p; }

                //ansi reset
                enum RESET { 
reset           };
                inline Print&
                operator<< (Print& p, RESET) { return ansi_ ? p << cls << home << normal : p; }


                //svg colors
                static constexpr Rgb ALICE_BLUE               { 240,248,255 };
                static constexpr Rgb ANTIQUE_WHITE            { 250,235,215 };
                static constexpr Rgb AQUA                     { 0,255,255 };
                static constexpr Rgb AQUAMARINE               { 127,255,212 };
                static constexpr Rgb AZURE                    { 240,255,255 };
                static constexpr Rgb BEIGE                    { 245,245,220 };
                static constexpr Rgb BISQUE                   { 255,228,196 };
                static constexpr Rgb BLACK                    { 0,0,0 };
                static constexpr Rgb BLANCHED_ALMOND          { 255,235,205 };
                static constexpr Rgb BLUE                     { 0,0,255 };
                static constexpr Rgb BLUE_VIOLET              { 138,43,226 };
                static constexpr Rgb BROWN                    { 165,42,42 };
                static constexpr Rgb BURLY_WOOD               { 222,184,135 };
                static constexpr Rgb CADET_BLUE               { 95,158,160 };
                static constexpr Rgb CHARTREUSE               { 127,255,0 };
                static constexpr Rgb CHOCOLATE                { 210,105,30 };
                static constexpr Rgb CORAL                    { 255,127,80 };
                static constexpr Rgb CORNFLOWER_BLUE          { 100,149,237 };
                static constexpr Rgb CORNSILK                 { 255,248,220 };
                static constexpr Rgb CRIMSON                  { 220,20,60 };
                static constexpr Rgb CYAN                     { 0,255,255 };
                static constexpr Rgb DARK_BLUE                { 0,0,139 };
                static constexpr Rgb DARK_CYAN                { 0,139,139 };
                static constexpr Rgb DARK_GOLDEN_ROD          { 184,134,11 };
                static constexpr Rgb DARK_GRAY                { 169,169,169 };
                static constexpr Rgb DARK_GREEN               { 0,100,0 };
                static constexpr Rgb DARK_KHAKI               { 189,183,107 };
                static constexpr Rgb DARK_MAGENTA             { 139,0,139 };
                static constexpr Rgb DARK_OLIVE_GREEN         { 85,107,47 };
                static constexpr Rgb DARK_ORANGE              { 255,140,0 };
                static constexpr Rgb DARK_ORCHID              { 153,50,204 };
                static constexpr Rgb DARK_RED                 { 139,0,0 };
                static constexpr Rgb DARK_SALMON              { 233,150,122 };
                static constexpr Rgb DARK_SEA_GREEN           { 143,188,143 };
                static constexpr Rgb DARK_SLATE_BLUE          { 72,61,139 };
                static constexpr Rgb DARK_SLATE_GRAY          { 47,79,79 };
                static constexpr Rgb DARK_TURQUOISE           { 0,206,209 };
                static constexpr Rgb DARK_VIOLET              { 148,0,211 };
                static constexpr Rgb DEEP_PINK                { 255,20,147 };
                static constexpr Rgb DEEP_SKY_BLUE            { 0,191,255 };
                static constexpr Rgb DIM_GRAY                 { 105,105,105 };
                static constexpr Rgb DODGER_BLUE              { 30,144,255 };
                static constexpr Rgb FIRE_BRICK               { 178,34,34 };
                static constexpr Rgb FLORAL_WHITE             { 255,250,240 };
                static constexpr Rgb FOREST_GREEN             { 34,139,34 };
                static constexpr Rgb FUCHSIA                  { 255,0,255 };
                static constexpr Rgb GAINSBORO                { 220,220,220 };
                static constexpr Rgb GHOST_WHITE              { 248,248,255 };
                static constexpr Rgb GOLD                     { 255,215,0 };
                static constexpr Rgb GOLDEN_ROD               { 218,165,32 };
                static constexpr Rgb GRAY                     { 128,128,128 };
                static constexpr Rgb GREEN                    { 0,128,0 };
                static constexpr Rgb GREEN_YELLOW             { 173,255,47 };
                static constexpr Rgb HONEY_DEW                { 240,255,240 };
                static constexpr Rgb HOT_PINK                 { 255,105,180 };
                static constexpr Rgb INDIAN_RED               { 205,92,92 };
                static constexpr Rgb INDIGO                   { 75,0,130 };
                static constexpr Rgb IVORY                    { 255,255,240 };
                static constexpr Rgb KHAKI                    { 240,230,140 };
                static constexpr Rgb LAVENDER                 { 230,230,250 };
                static constexpr Rgb LAVENDER_BLUSH           { 255,240,245 };
                static constexpr Rgb LAWN_GREEN               { 124,252,0 };
                static constexpr Rgb LEMON_CHIFFON            { 255,250,205 };
                static constexpr Rgb LIGHT_BLUE               { 173,216,230 };
                static constexpr Rgb LIGHT_CORAL              { 240,128,128 };
                static constexpr Rgb LIGHT_CYAN               { 224,255,255 };
                static constexpr Rgb LIGHT_GOLDENROD_YELLOW   { 250,250,210 };
                static constexpr Rgb LIGHT_GRAY               { 211,211,211 };
                static constexpr Rgb LIGHT_GREEN              { 144,238,144 };
                static constexpr Rgb LIGHT_PINK               { 255,182,193 };
                static constexpr Rgb LIGHT_SALMON             { 255,160,122 };
                static constexpr Rgb LIGHT_SEA_GREEN          { 32,178,170 };
                static constexpr Rgb LIGHT_SKY_BLUE           { 135,206,250 };
                static constexpr Rgb LIGHT_SLATE_GRAY         { 119,136,153 };
                static constexpr Rgb LIGHT_STEEL_BLUE         { 176,196,222 };
                static constexpr Rgb LIGHT_YELLOW             { 255,255,224 };
                static constexpr Rgb LIME                     { 0,255,0 };
                static constexpr Rgb LIME_GREEN               { 50,205,50 };
                static constexpr Rgb LINEN                    { 250,240,230 };
                static constexpr Rgb MAGENTA                  { 255,0,255 };
                static constexpr Rgb MAROON                   { 128,0,0 };
                static constexpr Rgb MEDIUM_AQUAMARINE        { 102,205,170 };
                static constexpr Rgb MEDIUM_BLUE              { 0,0,205 };
                static constexpr Rgb MEDIUM_ORCHID            { 186,85,211 };
                static constexpr Rgb MEDIUM_PURPLE            { 147,112,219 };
                static constexpr Rgb MEDIUM_SEA_GREEN         { 60,179,113 };
                static constexpr Rgb MEDIUM_SLATE_BLUE        { 123,104,238 };
                static constexpr Rgb MEDIUM_SPRING_GREEN      { 0,250,154 };
                static constexpr Rgb MEDIUM_TURQUOISE         { 72,209,204 };
                static constexpr Rgb MEDIUM_VIOLET_RED        { 199,21,133 };
                static constexpr Rgb MIDNIGHT_BLUE            { 25,25,112 };
                static constexpr Rgb MINT_CREAM               { 245,255,250 };
                static constexpr Rgb MISTY_ROSE               { 255,228,225 };
                static constexpr Rgb MOCCASIN                 { 255,228,181 };
                static constexpr Rgb NAVAJO_WHITE             { 255,222,173 };
                static constexpr Rgb NAVY                     { 0,0,128 };
                static constexpr Rgb OLD_LACE                 { 253,245,230 };
                static constexpr Rgb OLIVE                    { 128,128,0 };
                static constexpr Rgb OLIVE_DRAB               { 107,142,35 };
                static constexpr Rgb ORANGE                   { 255,165,0 };
                static constexpr Rgb ORANGE_RED               { 255,69,0 };
                static constexpr Rgb ORCHID                   { 218,112,214 };
                static constexpr Rgb PALE_GOLDENROD           { 238,232,170 };
                static constexpr Rgb PALE_GREEN               { 152,251,152 };
                static constexpr Rgb PALE_TURQUOISE           { 175,238,238 };
                static constexpr Rgb PALE_VIOLET_RED          { 219,112,147 };
                static constexpr Rgb PAPAYA_WHIP              { 255,239,213 };
                static constexpr Rgb PEACH_PUFF               { 255,218,185 };
                static constexpr Rgb PERU                     { 205,133,63 };
                static constexpr Rgb PINK                     { 255,192,203 };
                static constexpr Rgb PLUM                     { 221,160,221 };
                static constexpr Rgb POWDER_BLUE              { 176,224,230 };
                static constexpr Rgb PURPLE                   { 128,0,128 };
                static constexpr Rgb REBECCA_PURPLE           { 102,51,153 };
                static constexpr Rgb RED                      { 255,0,0 };
                static constexpr Rgb ROSY_BROWN               { 188,143,143 };
                static constexpr Rgb ROYAL_BLUE               { 65,105,225 };
                static constexpr Rgb SADDLE_BROWN             { 139,69,19 };
                static constexpr Rgb SALMON                   { 250,128,114 };
                static constexpr Rgb SANDY_BROWN              { 244,164,96 };
                static constexpr Rgb SEA_GREEN                { 46,139,87 };
                static constexpr Rgb SEA_SHELL                { 255,245,238 };
                static constexpr Rgb SIENNA                   { 160,82,45 };
                static constexpr Rgb SILVER                   { 192,192,192 };
                static constexpr Rgb SKY_BLUE                 { 135,206,235 };
                static constexpr Rgb SLATE_BLUE               { 106,90,205 };
                static constexpr Rgb SLATE_GRAY               { 112,128,144 };
                static constexpr Rgb SNOW                     { 255,250,250 };
                static constexpr Rgb SPRING_GREEN             { 0,255,127 };
                static constexpr Rgb STEEL_BLUE               { 70,130,180 };
                static constexpr Rgb TAN                      { 210,180,140 };
                static constexpr Rgb TEAL                     { 0,128,128 };
                static constexpr Rgb THISTLE                  { 216,191,216 };
                static constexpr Rgb TOMATO                   { 255,99,71 };
                static constexpr Rgb TURQUOISE                { 64,224,208 };
                static constexpr Rgb VIOLET                   { 238,130,238 };
                static constexpr Rgb WHEAT                    { 245,222,179 };
                static constexpr Rgb WHITE                    { 255,255,255 };
                static constexpr Rgb WHITE_SMOKE              { 245,245,245 };
                static constexpr Rgb YELLOW                   { 255,255,0 };
                static constexpr Rgb YELLOW_GREEN             { 154,205,50 };

                // can adjust colors by multiplication (at compile time)
                // no need to get more complicated with other operators as this is just
                // a simple way to brighten/darken an existing named color
                // WHITE * 0.5
                static constexpr Rgb 
operator *      (const Rgb& r, const double v)
                {
                return Rgb{ static_cast<u8>(r.r*v), static_cast<u8>(r.g*v), static_cast<u8>(r.b*v) };
                }

//========================================================================================
                } //namespace FMT::ANSI
//========================================================================================
                } //namespace FMT
//========================================================================================



                // additional opperator << types can either be added here
                // or can be added in a source/header file where needed 
                // (which will then need to #include "Print.hpp")
                //
                // put these in the FMT namespace so that the operator, will
                // be found and there will be no need to create an operator, version
                // (which would simply redirect to the operator<< version)
//========================================================================================
namespace
FMT
//========================================================================================
                {

                //Rgb colors, decimal or hex output 
                //(will be decimal unless currently in hex mode)
                inline FMT::Print&   
operator<<      (FMT::Print& p, const FMT::ANSI::Rgb rgb)
                {
                using namespace FMT;
                auto saveBase{ p.base() };//save/restore base (we may be using dec)
                if( saveBase == hex ){ //if we are in hex mode, use #hex style
                    return p, '#', Hex0(2,rgb.r), Hex0(2,rgb.g), Hex0(2,rgb.b); //#FF00FF
                    }
                return p, dec, rgb.r, '.', rgb.g, '.', rgb.b, saveBase; //255.0.255
                }

//========================================================================================
                } //namespace FMT
//========================================================================================
