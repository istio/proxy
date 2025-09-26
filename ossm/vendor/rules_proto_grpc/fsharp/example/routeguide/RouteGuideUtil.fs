module RouteGuideUtil

open Newtonsoft.Json
open System
open System.Collections.Generic
open System.IO
open RouteGuide

type JsonLocation = { Longitude: int; Latitude: int }

type JsonFeature =
    { Name: string
      Location: JsonLocation }

let private coordFactor : double = 1e7

let toRadians (value: double) : double = (Math.PI / (180 |> double)) * value

let getLatitude (point: Point) = (point.Latitude |> double) / coordFactor

let getDistance (start: Point) (``end``: Point) : double =
    let r = 6371000.
    let lat1 = toRadians (start.Latitude |> double)
    let lat2 = toRadians (``end``.Latitude |> double)
    let lon1 = toRadians (start.Longitude |> double)
    let lon2 = toRadians (``end``.Longitude |> double)
    let deltalat = lat2 - lat1
    let deltalon = lon2 - lon1

    let a =
        Math.Sin(deltalat / 2.) * Math.Sin(deltalat / 2.)
        + Math.Cos(lat1)
            * Math.Cos(lat2)
            * Math.Sin(deltalon / 2.)
            * Math.Sin(deltalon / 2.)

    let c =
        2. * Math.Atan2(Math.Sqrt(a), Math.Sqrt(1. - a))

    r * c

let contains (rectangle: Rectangle) (point: Point) =
    match (rectangle.Lo, rectangle.Hi) with
    | (ValueSome (recLo), ValueSome (recHi)) ->
        let left = Math.Min(recLo.Longitude, recHi.Longitude)
        let right = Math.Max(recLo.Longitude, recHi.Longitude)
        let top = Math.Max(recLo.Latitude, recHi.Latitude)
        let bottom = Math.Min(recLo.Latitude, recHi.Latitude)

        (point.Longitude >= left
            && point.Longitude <= right
            && point.Latitude >= bottom
            && point.Latitude <= top)
    | _ -> false

let jsonFeatureToProtoFeature (jsonFeature: JsonFeature) : Feature =
    { Name = jsonFeature.Name
      Location =
          ValueSome(
              { Latitude = jsonFeature.Location.Latitude
                Longitude = jsonFeature.Location.Longitude
                _UnknownFields = null }
          )
      _UnknownFields = null }

let parseFeatures filename : Feature seq =
    JsonConvert.DeserializeObject<List<JsonFeature>>(File.ReadAllText(filename))
    |> Seq.map jsonFeatureToProtoFeature
