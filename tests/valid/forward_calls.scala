import ifj26.*
@main def main(): Unit =
  print(twice(4))
  print(isEven(10))
def twice(x: Int) = x * 2
def isEven(n: Int): Boolean = if n == 0 then true else isOdd(n - 1)
def isOdd(n: Int): Boolean = if n == 0 then false else isEven(n - 1)
