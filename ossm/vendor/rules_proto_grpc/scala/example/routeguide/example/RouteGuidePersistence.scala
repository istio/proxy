package example

import java.net.URL
import java.util.logging.Logger

import scala.io.Source

import io.grpc.examples.routeguide.routeguide.{Feature, Point}

object RouteGuidePersistence {
  val logger: Logger = Logger.getLogger(getClass.getName)

  val defaultFeatureFile: URL = getClass.getClassLoader.getResource("scala/example/routeguide/route_guide_db.json")

  /**
    * Get a canned sequence of features so we don't have to parse the json file.
    */
  def getFeatures(): Seq[Feature] = {
    val features: Seq[Feature] = Seq(
      Feature(
        name = "Patriots Path, Mendham, NJ 07945, USA",
        location = Some(Point(407838351, -746143763))),
      Feature(
        name = "101 New Jersey 10, Whippany, NJ 07981, USA",
        location = Some(Point(408122808, -743999179)))
    )
    features
  }

}
