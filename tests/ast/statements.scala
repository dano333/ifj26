import ifj26.*
extension (s: String)
  def shout(): String = s + "!"
  def twice(n: Int): String = s + s
def id(v: Int): Int =
  if v > 0 then
    return v
  else if v < 0 then
    return -v
  else
    return 0
def x1(): Int = 1
@main def main(): Unit =
  var a = 10
  var b = 3
  val c = a - b - 1
  val d = a - (b - 1)
  val e = 1 < 2 == true
  val f = a * -b
  val g = "x" == "y"
  val h = x1()
  val s = "hi".shout()
  val n = a.toStr.strLength
  val u = ()
  val q = if a > b then
    val z = a
    z
  else
    b
  for i <- 1 to a by 2 if i > 1 if i < 9 do
    print(i)
  while a > 0 do
    a = a - 1
    if a == 5 then print("five")
  print("\n")
