/*

Node: prepare for database

Function:
    Arrange the experimental data into a form acceptable to InfluxDB
    
*/

// we start by creating an object that will be the result (and the input
// to InfluxDB in the next stage).
//
// It must have the field `payload`.
//
// `payload` must be an array of two objects. It effectively defines a
//      row in the data table.
// `payload[0]` defines the *fields* in this measurement.  Fields in InfluxDB are
//      data that is aggregated, averaged, summed, etc. They're really
//      "measurements".  Fields are *not* indexed, however; you can't
//      efficiently query on a field value.
// `payload[1]` defines the *tags* to be associated with a measurment.
//      You can't do computations on tags, but tags are indexed, so it's
//      very efficient to retreive all measurements with a given tag (or
//      tags).
//
// We create in two steps; first do all the work that is really independent
// of the experiement; then we fill in the experiement-specific data.

var result =
{
    payload:
        [{
            // [0] specifies fields
            // we always record the message ID and the counter as fields.
            msgID: msg._msgid,
            counter: msg.counter,
            //time: new Date(msg.metadata.time).getTime(),
        },
        {
            // [1] specifies tags
            // we always record a lot of metadata as tags:
            devEUI: msg.hardware_serial,
            devID: msg.dev_id,
            displayName: msg.display_id,
            displayKey: msg.display_key,
            nodeType: msg.local.nodeType,
            platformType: msg.local.platformType,
            radioType: msg.local.radioType,
            applicationName: msg.local.applicationName,
        }]
};

// to save typing, we copy the field object to variable t. This is
// done by reference, so when we change t, we also change result.payload[0].
var pFields = result.payload[0];

// similary, we copy the tags object to variable tags.
var pTags = result.payload[1];

// this variable lists the names of msg.payload data fields that will be put into
// the measurement as fields.
var field_keys = [ 
    "vBat", "vBus", "vSys", "boot", "tempC", "tDewC", "tHeatIndexC", "p", "p0", "rh", "irradiance", "activity"
    ];

// this variable lists the names of msg.payload data fields that will be put into
// the measurement as tags.
var tag_keys = [
    /* none */
    ];

// this local function takes a value or object from the input data,
// and puts it in the output object. If the input value is an object,
// it is flattened by creating output labels of the form "a.b.c.d",
// where "a" comes from the sInkey param, and "b.c.d" are computed by
// recursively traversing the inValue until we reach a leaf.
function insert_value(pOutput, sInKey, inValue)
    {
    if (typeof inValue == "object" )
        {
        // complex object: flatten it.
        for (var i in inValue)
            insert_value(pOutput, sInKey + "." + i, inValue[i]);
        }
    else
        // simple object, just insert it.
        pOutput[sInKey] = inValue;
    }

// now that we have the infrastructure prepared, we can copy all of the
// desired field values into result.payload[0] (via pFields)
for (var i in field_keys)
    {
    var key = field_keys[i];
    if (key in msg.payload)
        {
        // if we get an object generate an entry for each
        insert_value(pFields, key, msg.payload[key]);
        }
    }

// copy the desired tag values
for (var i in tag_keys)
    {
    var key = tag_keys[i];
    if (key in msg.payload)
        pTags[key] = msg.payload[key];
    }

// result is now ready to forward.
return result;
