import ifj26.*
def twice(x: Int): Int = x * 2
@main def main(): Unit =
  var a = 1
  if a > 0 then
    val t = 5
    print(t)
  else
    val t = 6
    print(t)
  while a < 3 do
    val t = a
    a = a + 1
  for i <- 1 to 3 do
    print(i)
  for j <- 0 until 10 by 2 if j > 2 do
    print(j)
  print(twice(a))
