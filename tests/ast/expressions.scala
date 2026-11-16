import ifj26.*
def add(a: Int)(b: Int, c: Double): Double = a + b * c
@main def main(): Unit =
  var x = 1 + 2 * 3 - 4 / 2
  val d: Double = 1
  val m = -x * 2 + 1
  val b = !(x == 1) && x < 5 || false
  val r = add(1)(2, 3.5) + 1
  val l = strLength("abc") - 1
  val s = x.toStr
  val t = "a" + "b"
  val e = if x > 1 then 1 else 2.5
  x = x + 1
  print(x)
