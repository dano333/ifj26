import ifj26.*
extension (s: String)
  def shout(): String = s + "!"
extension (n: Int)
  def shout(): String = "int"
@main def main(): Unit =
  val k = 1
  print("x".shout() + k.shout())
