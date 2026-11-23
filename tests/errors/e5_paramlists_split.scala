import ifj26.*
def add(a: Int, b: Int)(c: Int): Int = a + b + c
@main def main(): Unit =
  print(add(1)(2, 3))
