import ifj26.*
@main def main(): Unit =
  val k = 3
  print(k.triple)
  print(k.plus(4))
extension (n: Int)
  def triple: Int = n * 3
  def plus(m: Int): Int = n + m
