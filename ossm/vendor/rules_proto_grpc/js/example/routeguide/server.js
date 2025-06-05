const grpc = require('@grpc/grpc-js');

const messages = require('routeguide_package/example/proto/routeguide_pb.js')
const services = require('routeguide_package/example/proto/routeguide_grpc_pb.js')
const featureDb = require('rules_proto_grpc/example/proto/routeguide_features.json');

const COORD_FACTOR = 1e7;
let feature_list = [];


function checkFeature(point) {
    // Check if there is already a feature object for the given point
    for (const feature of feature_list) {
        if (feature.getLocation().getLatitude() === point.getLatitude() &&
        feature.getLocation().getLongitude() === point.getLongitude()) {
            return feature;
        }
    }

    // Create empty feature
    const name = '';
    const feature = new messages.Feature();
    feature.setName(name);
    feature.setLocation(point);
    return feature;
}


function getFeature(call, callback) {
    callback(null, checkFeature(call.request));
}


function listFeatures(call) {
    const lo = call.request.getLo();
    const hi = call.request.getHi();
    const left = Math.min([lo.getLongitude(), hi.getLongitude()]);
    const right = Math.max([lo.getLongitude(), hi.getLongitude()]);
    const top = Math.max([lo.getLatitude(), hi.getLatitude()]);
    const bottom = Math.min([lo.getLatitude(), hi.getLatitude()]);

    // For each feature, check if it is in the given bounding box
    feature_list.forEach((feature) => {
        if (feature.getName() === '') return;
        if (feature.getLocation().getLongitude() >= left &&
        feature.getLocation().getLongitude() <= right &&
        feature.getLocation().getLatitude() >= bottom &&
        feature.getLocation().getLatitude() <= top) {
            call.write(feature);
        }
    });

    call.end();
}


function getDistance(start, end) {
    function toRadians(num) {
        return num * Math.PI / 180;
    }

    const R = 6371000;  // earth radius in metres
    const lat1 = toRadians(start.getLatitude() / COORD_FACTOR);
    const lat2 = toRadians(end.getLatitude() / COORD_FACTOR);
    const lon1 = toRadians(start.getLongitude() / COORD_FACTOR);
    const lon2 = toRadians(end.getLongitude() / COORD_FACTOR);

    const deltalat = lat2-lat1;
    const deltalon = lon2-lon1;
    const a = Math.sin(deltalat / 2) * Math.sin(deltalat / 2) +
        Math.cos(lat1) * Math.cos(lat2) *
        Math.sin(deltalon / 2) * Math.sin(deltalon / 2);
    const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    return R * c;
}


function recordRoute(call, callback) {
    let pointCount = 0;
    let featureCount = 0;
    let distance = 0;
    let previous = null;

    // Start a timer
    const start_time = process.hrtime();
    call.on('data', (point) => {
        pointCount++;
        if (checkFeature(point).name !== '') featureCount += 1;
        if (previous != null) distance += getDistance(previous, point);
        previous = point;
    });

    call.on('end', () => {
        const summary = new messages.RouteSummary();
        summary.setPointCount(pointCount);
        summary.setFeatureCount(featureCount);
        summary.setDistance(distance|0);
        summary.setElapsedTime(process.hrtime(start_time)[0]);
        callback(null, summary);
    });
}


const route_notes = new Map();

function pointKey(point) {
  return point.getLatitude() + ' ' + point.getLongitude();
}


function routeChat(call) {
    call.on('data', function(note) {
        const key = pointKey(note.getLocation());
        if (route_notes.has(key)) {
            route_notes[key].forEach((note) => {
                call.write(note);
            });
        } else {
            route_notes[key] = [];
        }

        // Then add the new note to the list
        route_notes[key].push(note);
    });

    call.on('end', call.end);
}


if (require.main === module) {
    let port = '50051';
    if (process.env.SERVER_PORT) port = process.env.SERVER_PORT;
    const addr = '0.0.0.0:' + port;
    const routeServer = new grpc.Server();
    routeServer.addService(services.RouteGuideService, {
        getFeature: getFeature,
        listFeatures: listFeatures,
        recordRoute: recordRoute,
        routeChat: routeChat
    });

    routeServer.bindAsync(addr, grpc.ServerCredentials.createInsecure(), () => {
        // Transform the loaded features to Feature objects
        feature_list = featureDb.map(function(value) {
            const feature = new messages.Feature();
            feature.setName(value.name);
            const location = new messages.Point();
            location.setLatitude(value.location.latitude);
            location.setLongitude(value.location.longitude);
            feature.setLocation(location);
            return feature;
        });

        console.log(`Feature database contains ${feature_list.length} entries.`);
        console.log(`Node server listening at ${addr}...`)
        routeServer.start();
    });
}
