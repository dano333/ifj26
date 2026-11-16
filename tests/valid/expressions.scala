import ifj26.*
/* nested /* comment */ still comment */
def calc(a: Int, b: Double): Double =
  val x = -a * 2 + 1
  val y = (a + 1) * (2 - 3) / 4
  val z = b * 2 + a
  val ok = !(a == 1) && a < 5 || b >= 2.5
  if ok then
    return z + x
  else
    return y
@main def main(): Unit =
  val r = calc(3, 1.5)
  val s = if r > 1 then "big" else "small"
  print(s + "!")
  print(toStr(r))
