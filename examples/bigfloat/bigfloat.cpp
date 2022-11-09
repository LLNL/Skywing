#include <iostream>
#include "skywing_mid/big_float.hpp"

using skywing::BigFloat;

int main(int, char**)
{
  double d = 2.0;
  BigFloat bf(d);
  std::cout << "Double 2.0 to BigFloat and back is " << bf.to_double() << std::endl;

  d = 1e10;
  BigFloat bf2(d);
  std::cout << "Double 1e10 to BigFloat and back is " << bf2.to_double() << std::endl;

  d = 1e-10;
  BigFloat bf3(d);
  std::cout << "Double 1e-10 to BigFloat and back is " << bf3.to_double() << std::endl;

  std::cout << "Addition of bf with itself is " << (bf + bf).to_double() << std::endl;
  std::cout << "Subtraction of bf with itself is " << (bf - bf).to_double() << std::endl;
  std::cout << "Negation of bf is " << (-bf).to_double() << std::endl;

  std::cout << "Multiplication of bf with itself is " << (bf * bf).to_double() << std::endl;
  std::cout << "Division of bf with itself is " << (bf / bf).to_double() << std::endl;

  double c = 3.0;
  std::cout << "Multiplication of bf with 3.0 is " << (c * bf).to_double() << std::endl;
  std::cout << "Division of bf with 3.0 is " << (bf / c).to_double() << std::endl;

  BigFloat bf4(1e3);
  std::cout << "Addition of 2.0 and 1e3 is " << (bf + bf4).to_double() << std::endl;
  BigFloat bf5(1e-2);
  std::cout << "Addition of 2.0 and 1e-2 is " << (bf + bf5).to_double() << std::endl;

  std::cout << "2^2.0 is " << pow2(bf).to_double() << std::endl;
  std::cout << "2^(2.0+2.0) is " << pow2(bf+bf).to_double() << std::endl;
  std::cout << "2^(2.0-2.0) is " << pow2(bf-bf).to_double() << std::endl;
  std::cout << "2^(-(2.0+2.0)) is " << pow2(-(bf+bf)).to_double() << std::endl;

  double d25 = 2.5;
  for (std::size_t i = 0; i < 600; i+=17)
  {
    double inp = d25 * i;
    BigFloat pb = exp(BigFloat(i*d25));
    BigFloat lb = log(pb);
    std::cout << "exp(" << inp << ") is " << exp(i*d25) << " and in BigFloat is " << pb;
    std::cout << " and its log is " << lb.to_double() << std::endl;
  }
  for (std::size_t i = 0; i < 600; i+=17)
  {
    double inp = -d25 * i;
    BigFloat pb = exp(BigFloat(inp));
    BigFloat lb = log(pb);
    std::cout << "exp(" << inp << ") is " << exp(inp) << " and in BigFloat is " << pb;
    std::cout << " and its log is " << lb.to_double() << std::endl;
  }

  BigFloat mybf1 = exp(BigFloat(-d25 * 600 * 17));
  BigFloat mybf2 = exp(BigFloat(-d25 * 17));
  std::cout << mybf1 << " + " << mybf2 << " = " << (mybf1 + mybf2) << std::endl;

  std::cout << "BigFloat(0.0) is " << BigFloat((double)0.0) << std::endl;

  int exp;
  double frac = std::frexp(0.0, &exp);
  std::cout << "frexp(0.0) is " << frac << ", " << exp << std::endl;
}
