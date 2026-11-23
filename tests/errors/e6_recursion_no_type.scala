import ifj26.*
def f(n: Int) = if n == 0 then 0 else f(n - 1)
@main def main(): Unit =
  print(f(3))
