/* settings */
var common_edge_postfix = "bit/s";
var common_node_prefix  = "192.168.0.";
var netjsonurl = "http://169.254.0.101/cgi/netjson.cgi";

var selectedColor = {
    border:     '#2BE97C',
    background: '#D2FFE5',
    highlight: {
        border: '#2BE97C',
        background: '#D2FFE5',
    },
};

/* global variables */
var visNodes, visEdges, visNetwork;
var autoupdate_timeout;
var xmlhttp;

String.prototype.startsWith = function (pattern) {
    if (this.length < pattern.length) {
        return false;
    }
    return pattern == this.substring(0, pattern.length);
};

String.prototype.endsWith = function (pattern) {
    if (this.length < pattern.length) {
        return false;
    }
    return pattern == this.substring(this.length - pattern.length);
};

function init_network() {
    var options = {
    };

    // create an array with nodes
    visNodes = new vis.DataSet([], options);

    // create an array with edges
    visEdges = new vis.DataSet([], options);

    // create a network
    var container = document.getElementById("networkgraph");
    var visData = {
        nodes: visNodes,
        edges: visEdges
    };

    options = {
        edges: {
            smooth: {
                type: 'continuous'
            }
        },
    };
    visNetwork = new vis.Network(container, visData, options);
  
    firstLookup = true;
}

function layout_nodes(element) {
    var jsonNodes = element.nodes;
    var newIds = []
    var oldIds = []
    
    /* add new nodes */
    newIds = []
    oldIds = visNodes.getIds()
    for (ni = 0; ni < jsonNodes.length; ni++) {
        var nId = jsonNodes[ni].id;
        var nColor = null;
        
        var nLabel = nId;
        if (nId.startsWith(common_node_prefix)) {
            nLabel = nId.substring(common_node_prefix.length);
        }
                        
        newIds.push(nId);
        if (jsonNodes[ni].id == element.router_id) {
            nColor = selectedColor;
        }

        if (!visNodes.get(nId)) {   
            visNodes.add(
                {
                    id:    nId,
                    label: nLabel,
                    mass:  4,
                    color: nColor,
                }
            );
        }
    }
    
    /* remove old nodes */
    for (ni = 0; ni < oldIds.length; ni++) {
        if (newIds.indexOf(oldIds[ni]) == -1) {
            visNodes.remove(oldIds[ni]);
        }
    }
}

function layout_edges(element) {
    var jsonEdges = element.links;
    var newIds = []
    var oldIds = []
    var undirectedEdges = {}
    
    /* calculate unidirectional edges */
    newIds = []
    oldIds = visEdges.getIds()
    for (ei = 0; ei < jsonEdges.length; ei++) {
        var eFrom = jsonEdges[ei].source;
        var eTo = jsonEdges[ei].target;
        
        if (eFrom > eTo) {
            var tmp = eTo;
            eTo = eFrom;
            eFrom = tmp;
        }

        var uuid = eFrom + "-" + eTo;
        
        var edge = undirectedEdges[uuid];
        if (edge == null) {
            edge = {
                uuid:      eFrom + "-" + eTo,
                from:      eFrom,
                to:        eTo,
                width:     1,
                arrows:    "",
                fromLabel: "-",
                toLabel:   "-"
            };
            undirectedEdges[uuid] = edge;
        };
                
        newIds.push(edge.uuid);
        
        var eLabel =  jsonEdges[ei].weight.toString();
        if (jsonEdges[ei].properties) {
            if (jsonEdges[ei].properties["weight_txt"]) {
                eLabel = jsonEdges[ei].properties["weight_txt"];
            }
            if (jsonEdges[ei].properties["outgoing_tree"] == "true") {
                edge.width=3;
                if (jsonEdges[ei].source == eFrom) {
                    edge.arrows = "to";
                }
                else {
                    edge.arrows = "from";
                }
            }
        }
        
        if (jsonEdges[ei].source == eFrom) {
            edge.fromLabel = eLabel;
        }
        else {
            edge.toLabel = eLabel;
        }
    }
    
    /* add new edges */
    for (ei = 0; ei < newIds.length; ei++) {
        var edge = undirectedEdges[newIds[ei]];
        var label = "";
        
        if (edge.fromLabel == edge.toLabel) {
            label = edge.fromLabel;
        }
        else if (edge.fromLabel.endsWith(common_edge_postfix)
                && edge.fromLabel.endsWith(common_edge_postfix)) {
            var flen = edge.fromLabel.length - common_edge_postfix.length;
            var tlen = edge.toLabel.length - common_edge_postfix.length;
        
            label = edge.fromLabel.substring(0, flen).trim() + "/"
                + edge.toLabel.substring(0, tlen).trim()
                + " " + common_edge_postfix;             
        }
        else {
            label = edge.fromLabel + "/" + edge.toLabel;
        }
        
        if (visEdges.get(edge.uuid)) {
            visEdges.update(
                {
                    id:          edge.uuid,
                    label:       label,
                    width:       edge.width,
                    arrows:      edge.arrows,
                }
            );
        }
        else {
            visEdges.add(
                {
                    id:          edge.uuid,
                    from:        edge.from,
                    to:          edge.to,
                    label:       label,
                    length:      200,
                    width:       edge.width,
                    arrows:      edge.arrows,
                    font: {
                        align:       "top",
                    }
                }
            );
        }
    }

    /* remove old edges */
    for (ei = 0; ei < oldIds.length; ei++) {
        if (newIds.indexOf(oldIds[ei]) == -1) {
            visEdges.remove(oldIds[ei]);
        }
    }
}

function xmlhttp_changed()
{
    if (xmlhttp.readyState==XMLHttpRequest.DONE && xmlhttp.status==200) {
        console.log("Got update");
        
        obj = JSON.parse(xmlhttp.responseText);

        if (obj.type != "NetworkCollection") {
            return;
        }

        for (index = 0; index < obj.collection.length; index++) {
            element = obj.collection[index];

            if (element.router_id.indexOf(':') != -1) {
                continue;
            }

            if (element.type == "NetworkGraph") {
                layout_nodes(element);
                layout_edges(element);
            }
        }
        
        var checkbox = document.getElementById("autoupdate");
        if (checkbox.checked) {
            autoupdate_timeout = window.setTimeout(send_request, 5000);
        }
    }
}

function autoupdate_clicked() {
    var checkbox = document.getElementById("autoupdate");
    if (checkbox.checked === false) {
        window.clearTimeout(autoupdate_timeout);
    }
    else {
        send_request();
    }    
}

function dynamiclayout_clicked() {
    var checkbox = document.getElementById("dynamiclayout");
    
    visNetwork.setOptions({physics:checkbox.checked})
}

function send_request() {
    xmlhttp.open("GET",netjsonurl,true);
    xmlhttp.send();
}

function init_netjson_viewer() {
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange=xmlhttp_changed
    init_network();
    send_request();
}
