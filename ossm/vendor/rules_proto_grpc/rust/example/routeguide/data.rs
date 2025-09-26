// https://github.com/hyperium/tonic/blob/4e5c6c8f2354301aed984da891ef284d16c936e5/examples/src/routeguide/data.rs

use routeguide_tonic::routeguide;
use serde::Deserialize;
use std::fs::File;

#[derive(Debug, Deserialize)]
struct Feature {
    location: Location,
    name: String,
}

#[derive(Debug, Deserialize)]
struct Location {
    latitude: i32,
    longitude: i32,
}

#[allow(dead_code)]
pub fn load() -> Vec<routeguide::Feature> {
    let file =
        File::open("example/proto/routeguide_features.json").expect("failed to open data file");

    let decoded: Vec<Feature> =
        serde_json::from_reader(&file).expect("failed to deserialize features");

    decoded
        .into_iter()
        .map(|feature| routeguide::Feature {
            name: feature.name,
            location: Some(routeguide::Point {
                longitude: feature.location.longitude,
                latitude: feature.location.latitude,
            }),
        })
        .collect()
}
