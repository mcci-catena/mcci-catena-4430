/*

Name:   format-22-influxdb-prep-data.js

Function:
    Decode port 0x01 format 0x22 messages for Node-RED.

Copyright and License:
    See accompanying LICENSE file at https://github.com/mcci-catena/MCCI-Catena-4430/

Author:
    Terry Moore, MCCI Corporation   October 2019

*/

// convert byte array to hexadecimal.
function toHexBuffer(b) {
    var result = "";
    for (var i = 0; i < b.length; ++i)
        result += ("0" + b[i].toString(16)).slice(-2);
    return result;
}

// create a pair for "fields" and "tags" at specified time. The time
// is given as Tposix * 1e9 (i.e., nanosecs before/after the Posix Epoch,
// idealized with every day being exactly 86400 seconds.
function initFieldsAndTags(msg, time) {
    var result =
        [
            // [0] is fields.
            {
                msgID: msg._msgid,
                time: time * 1e6
            },
            // [1] is tags.
            {
                devEUI: msg.hardware_serial,
                devID: msg.dev_id,
                displayName: msg.display_id,
                displayKey: msg.display_key,
                nodeType: msg.local.nodeType,
                platformType: msg.local.platformType,
                radioType: msg.local.radioType,
                applicationName: msg.local.applicationName,
            }
        ];
    return result;
}

// prepare the overall result.
var result = { payload: [] };

// the first entry (which will become the last after we deal with activity)
// gets most of the data.
result.payload[0] = initFieldsAndTags(msg, msg.payload.time.getTime());

// get references to the fields and tags of first entry.
var fields = result.payload[0][0];
var tags = result.payload[0][1];

// store the raw message in hex, in case it's ever needed.
fields.message_raw = toHexBuffer(msg.payload_raw);

// array of names of items in msg.payload, each match to be copied as a value.
var field_keys = [
    "counter", "Vbat", "Vbus", "Vsys", "boot", "tempC", "tDewC", "tHeatIndexC", "p", "p0", "rh", "irradiance", "pellets", "activity"
];

// array of names of items in msg.payload, each match to be copied as a tag
var tag_keys = [
    /* none */
];

// function to flatten the inValue, if needed, by createing multiple suitable
// string keys until a non-array and non-object is reached.
function insert_value(pOutput, sInKey, inValue) {
    if (Array.isArray(inValue)) {
        for (var i = 0; i < inValue.length; ++i)
            insert_value(pOutput, sInKey + "[" + i.toString() + "]", inValue[i]);
    }
    else if (typeof inValue == "object") {
        for (var i in inValue)
            insert_value(pOutput, sInKey + "." + i, inValue[i]);
    }
    else
        pOutput[sInKey] = inValue;
}

// process each of the listed fields from msg.payload.
for (var i in field_keys) {
    var key = field_keys[i];
    if (key in msg.payload) {
        // activity is a special case.
        if (key == 'activity' && msg.payload.activity.length > 0)
            fields.activity = msg.payload.activity[0];
        else
            // if we get an object generate an entry for each
            insert_value(fields, key, msg.payload[key]);
    }
}

// process each of the listed tags from msg.payload.
for (var i in tag_keys) {
    var key = tag_keys[i];
    if (key in msg.payload)
        tags[key] = msg.payload[key];
}

// now create points for the activity measurements. Each one was taken one
// minute previously (going back in time).
if ("activity" in msg.payload) {
    for (var i = 1; i < msg.payload.activity.length; ++i) {
        var thisPoint = initFieldsAndTags(
                            msg,
                            // back up the right number of minutes.
                            // time is in milliseconds.
                            msg.payload.time.getTime() - i * 60 * 1000
                            );
        result.payload[i] = thisPoint;

        insert_value(thisPoint[0], 'activity', msg.payload.activity[i]);
    }
}

// we think influxdb will work better wth ascending
// timestampes, so reverse the array as our last step.
result.payload.reverse();
return result;
