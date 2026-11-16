import ifj26.*
extension (s: String)
  def shout(): String = s + "!"
@main def main(): Unit =
  val a = 5
  val b = a.shout()
