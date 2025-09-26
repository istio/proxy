const util = require('util');
const grpc = require('@grpc/grpc-js');

const messages = require('routeguide/routeguide_pb/example/proto/routeguide_pb.js')
const services = require('routeguide/routeguide_pb/example/proto/routeguide_grpc_pb.js')

// This is included as data in the client, so we can load this database as a constant.
const featureList = require('rules_proto_grpc/example/proto/routeguide_features.json');
console.log(`Loaded ${featureList.length} from feature database`);
const COORD_FACTOR = 1e7;


function newPoint(latitude, longitude) {
    const point = new messages.Point()
    point.setLatitude(latitude);
    point.setLongitude(longitude);
    return point;
}

function newRectangle(lo, hi) {
    const rect = new messages.Rectangle()
    rect.setLo(lo);
    rect.setHi(hi);
    return rect;
}

function newNote(point, message) {
    const note = new messages.RouteNote()
    note.setLocation(point);
    note.setMessage(message);
    return note;
}


async function runGetFeature(client) {
    const method = util.promisify(client.getFeature).bind(client);
    for (const point of [
        newPoint(409146138, -746188906),
        newPoint(1, 1)
    ]) {
        const feature = await method(point);
        if (feature.getName() && feature.getName() != "undefined") {
            console.log(
                'Found feature called "' + feature.getName() + '" at ' +
                feature.getLocation().getLatitude() / COORD_FACTOR + ', ' +
                feature.getLocation().getLongitude() / COORD_FACTOR
            );
        } else {
            console.log('Found no feature');
        }
    }
}


function runListFeatures(client) {
    return new Promise((resolve, reject) => {
        const rectangle = newRectangle(
            newPoint(400000000, -750000000),
            newPoint(420000000, -73000000)
        );
        console.log('Looking for features between 40, -75 and 42, -73');

        const call = client.listFeatures(rectangle);
        call.on('data', (feature) => {
            console.log(
                'Found feature called "' + feature.getName() + '" at ' +
                feature.getLocation().getLatitude() / COORD_FACTOR + ', ' +
                feature.getLocation().getLongitude() / COORD_FACTOR
            );
        });
        call.on('end', resolve);
    });
}


function runRecordRoute(client) {
    return new Promise((resolve, reject) => {
        const call = client.recordRoute((error, stats) => {
            if (error) return reject(error);
            console.log('Finished trip with', stats.getPointCount(), 'points');
            console.log('Passed', stats.getFeatureCount(), 'features');
            console.log('Traveled', stats.getDistance(), 'meters');
            console.log('It took', stats.getElapsedTime(), 'seconds');
            resolve();
        });

        for (let i = 0; i < 10; i++) {
            const randIndex = ~~(Math.random() * (featureList.length - 1))
            console.log("randomIndex", randIndex);
            const randomPointJson = featureList[randIndex];
            const randomPoint = newPoint(
                randomPointJson.location.latitude, randomPointJson.location.longitude
            )
            console.log("randomPoint", randomPointJson, randomPoint.toObject());
            call.write(randomPoint);
        }

        call.end();
    });
}


function runRouteChat(client) {
    return new Promise((resolve, reject) => {
        const call = client.routeChat();
        call.on('data', (note) => {
            console.log(
                'Got message "' + note.getMessage() + '" at ' +
                note.getLocation().getLatitude() + ', ' + note.getLocation().getLongitude()
            );
        });
        call.on('end', resolve);

        const notes = [
            newNote(newPoint(0, 0), 'First message'),
            newNote(newPoint(0, 1), 'Second message'),
            newNote(newPoint(1, 0), 'Third message'),
            newNote(newPoint(0, 0), 'Fourth message'),
        ]
        for (const note of notes) {
            console.log(
                'Sending message "' + note.getMessage() + '" at ' +
                note.getLocation().getLatitude() + ', ' + note.getLocation().getLongitude()
            );
            call.write(note);
        }

        call.end();
    });
}


async function main() {
    let port = '50051';
    if (process.env.SERVER_PORT) port = process.env.SERVER_PORT;
    const addr = 'localhost:' + port;
    const client = new services.RouteGuideClient(addr, grpc.credentials.createInsecure());

    client.waitForReady(4000, async () => {
        await runGetFeature(client);
        await runListFeatures(client);
        await runRecordRoute(client);
        await runRouteChat(client);
    });
}

if (require.main === module) {
    main().catch((e) => {
        console.log(e)
        process.exit(1);
    });
}
