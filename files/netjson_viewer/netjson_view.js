/* global variables */
var visNodes, visEdges, visNetwork;
var autoupdate_timeout, force_layout_timeout;
var xmlhttp;
var last_json_data = null;

var settings = {
    checkbox_autoupdate:     "",
    checkbox_dynamic_layout: "",
    checkbox_ipv4:           "",
    
    common_edge_postfix: "",
    common_node_prefix: {
        ipv4: "",
        ipv6: "",
    },
    netjsonurl: null,
    selectedColor: {
        border:     '#2BE97C',
        background: '#D2FFE5',
        highlight: {
            border: '#2BE97C',
            background: '#D2FFE5',
        },
    },

    autoupdate_interval: 5000,
    ipv4:                true,
    autoupdate:          true,
    dynamic_layout:      true,
};

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
        physics: settings["dynamic_layout"],
    };
    visNetwork = new vis.Network(container, visData, options);
  
    firstLookup = true;
}

function layout_nodes(element) {
    var jsonNodes = element.nodes;
    var newIds = []
    var oldIds = []
    
    var prefix = "";
    if (settings["ipv4"]) {
        prefix = settings["common_node_prefix"]["ipv4"];
    }
    else {
        prefix = settings["common_node_prefix"]["ipv6"];
    }
    
    /* add new nodes */
    newIds = []
    oldIds = visNodes.getIds()
    for (ni = 0; ni < jsonNodes.length; ni++) {
        var nId = jsonNodes[ni].id;
        var nColor = null;
        
        var nLabel = nId;
        if (nId.startsWith(prefix)) {
            nLabel = nId.substring(prefix.length);
        }
                        
        newIds.push(nId);
        if (jsonNodes[ni].id == element.router_id) {
            nColor = settings["selectedColor"];
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

function layout_json_data(obj) {
    if (obj.type != "NetworkCollection") {
        return;
    }

    for (index = 0; index < obj.collection.length; index++) {
        element = obj.collection[index];

        if (element.type == "NetworkGraph") {
            if (settings["ipv4"] && element.router_id.indexOf(':') != -1) {
                continue;
            }
            if (!settings["ipv4"] && element.router_id.indexOf('.') != -1) {
                continue;
            }
            layout_nodes(element);
            layout_edges(element);
            break;
        }
    }
}
    
function xmlhttp_changed()
{
    if (xmlhttp.readyState==XMLHttpRequest.DONE && xmlhttp.status==200) {
        last_json_data = JSON.parse(xmlhttp.responseText);
        layout_json_data(last_json_data);

        if (settings["autoupdate"]) {
            autoupdate_timeout = window.setTimeout(
                    send_request, settings["autoupdate_interval"]);
        }
    }
}

function autoupdate_clicked() {
    settings["autoupdate"] = this.checked;
    if (this.checked === false) {
        window.clearTimeout(autoupdate_timeout);
    }
    else {
        send_request();
    }    
}

function dynamiclayout_clicked() {
    settings["dynamic_layout"] = this.checked;
    visNetwork.setOptions({physics:this.checked});
    if (force_layout_timeout) {
        window.clearTimeout(force_layout_timeout);
    }
}

function ipv4_clicked() {
    settings["ipv4"] = this.checked;
    if (last_json_data !== null) {
        layout_json_data(last_json_data);
        if (!settings["dynamic_layout"]) {
            visNetwork.setOptions({physics:true});
            visNetwork.stabilize();
            force_layout_timeout = window.setTimeout(
                function() { visNetwork.setOptions({physics:false}); }, 500);
        }
    }
}

function send_request() {
    xmlhttp.open("GET",settings["netjsonurl"],true);
    xmlhttp.send();
}

function copy_settings(dst, src) {
    for (var p in dst) {
        if (dst.hasOwnProperty(p) && src.hasOwnProperty(p)) {
            if (dst[p] !== null && typeof src[p] !== typeof dst[p]) {
                continue;
            }
            
            if (src[p] !== null && typeof dst[p] === 'object' && typeof src[p] === 'object') {
                copy_settings(dst[p], src[p]);
            }
            else {
                dst[p] = src[p];
            }
        } 
    }              
}

function set_checkbox_function(checkbox_id, setting_name, f) {
    if (checkbox_id === null || settings[checkbox_id] == "") {
        return;
    }
    
    var checkbox = document.getElementById(settings[checkbox_id]);
    if (checkbox) {
        checkbox.onclick = f;
        checkbox.checked = settings[setting_name];
    }
}

function init_netjson_viewer(custom_settings) {
    if (custom_settings !== null) {
        copy_settings(settings, custom_settings);
    }
    if (settings["netjsonurl"] === null) {
        console.log("Netjson URL setting missing");
        return;
    }
    
    set_checkbox_function("checkbox_autoupdate", "autoupdate", autoupdate_clicked);
    set_checkbox_function("checkbox_dynamic_layout", "dynamic_layout", dynamiclayout_clicked);
    set_checkbox_function("checkbox_ipv4", "ipv4", ipv4_clicked);
    
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange=xmlhttp_changed
    init_network();
    send_request();
}
