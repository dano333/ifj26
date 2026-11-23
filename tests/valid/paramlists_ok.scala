import ifj26.*
def add(a: Int, b: Int)(c: Int): Int = a + b + c
def zero: Int = 0
def one(): Int = 1
@main def main(): Unit =
  print(add(1, 2)(3))
  print(zero + one())
