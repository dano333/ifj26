import ifj26.*
def sign(a: Int): Int =
  if a > 0 then
    1
  else if a < 0 then
    -1
  else
    0
def half(a: Int): Double =
  a
@main def main(): Unit =
  print(sign(5))
  print(half(3))
