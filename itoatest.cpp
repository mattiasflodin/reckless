#include <cmath>

void foo(double value, unsigned significant_digits, char* str)
{
    // double has 53 bits of mantissa, i.e. 53 significant digits. In decimal,
    // that translates to 53*log(2)/log(10) = 15.95 significant digits.
    // Paraphrasing Kahan's "Lecture notes on the status of IEEE Standard 754
    // for Binary Floating-Point Arithmetic":
    //   If a Double Precision floating-point number is converted to a decimal
    //   string with at least 17 significant decimals and then converted back
    //   to Double, then the final number must match the original.
    // 
    // We need to verify whether 17 is actually required for some reason I
    // don't understand, but for now we'll try with 16 digits.
    
    //unsigned p = significant_digits;
    double log10 = std::log10(value);

    double power;
    if(log10<0)
        power = std::floor(log10);
    else
        power = std::ceil(log10);
    


}

int main()
{
    foo(12345678.0);
    return 0;
}
