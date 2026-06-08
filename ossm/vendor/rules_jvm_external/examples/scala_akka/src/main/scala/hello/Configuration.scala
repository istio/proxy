package hello

import pureconfig._
import pureconfig.generic.auto._
import scalaz.\/

case class ApplicationConfiguration(host: String, port: Int)

object Configuration {
  val parse: String \/ ApplicationConfiguration = {
    \/.fromEither(loadConfig[ApplicationConfiguration]("application"))
      .leftMap(f => s"Failed to load configuration due to: ${f.toString}")
  }
}
