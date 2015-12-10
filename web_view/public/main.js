jQuery(document).ready(function() {
    jQuery('.tabs .tab-links a').on('click', function(e)  {
        var currentAttrValue = jQuery(this).attr('href');
 
        // Show/Hide Tabs
        jQuery('.tabs ' + currentAttrValue).show().siblings().hide();
 
        // Change/remove current tab to active
        jQuery(this).parent('li').addClass('active').siblings().removeClass('active');
 
        e.preventDefault();
    });
});

function Node(node) {
    var self = this;
    self.nodeId = node.NodeId;
    self.currentDateVal = node.CurrentTime;
    var date = new Date(node.CurrentTime * 1000);
    self.currentTime = ko.observable(date.toLocaleTimeString() + " " + date.toLocaleDateString());
    self.parentId = ko.observable(node.ParentId);
    
    self.id = node.Id;
    self.isSink = ko.observable(node.IsSink);
    self.currentState = ko.observable(node.CurrentState);
    self.currentStateString = ko.computed(function() {
        var curStateBin = self.currentState().toString(2).split('').reverse().join('');
        var curState = []
        if(curStateBin.length == 1 && curStateBin.charAt(0) == '0')
            return "GOOD";
        
        if(curStateBin.charAt(0) == '1')
            curState.push("DATA_FAILURE");
        if(curStateBin.charAt(1) == '1')
            curState.push("PATH_FAILURE");
        if(curStateBin.charAt(2) == '1')
            curState.push("PATH_UPDATE");
        if(curStateBin.charAt(3) == '1')
            curState.push("IN_LINK_ERRORS_MINOR");
        if(curStateBin.charAt(4) == '1')
            curState.push("IN_LINK_ERRORS_MAJOR");
        if(curStateBin.charAt(5) == '1')
            curState.push("IN_LINK_FAILURE");
        if(curStateBin.charAt(6) == '1')
            curState.push("OUT_LINK_ERRORS_MINOR");
        if(curStateBin.charAt(7) == '1')
            curState.push("OUT_LINK_ERRORS_MAJOR");
        if(curStateBin.charAt(8) == '1')
            curState.push("OUT_LINK_FAILURE");
        if(curStateBin.charAt(9) == '1')
            curState.push("BATTERY_FAILURE");
        if(curStateBin.charAt(10) == '1')
            curState.push("REBOOTED");
        
        return curState.join();
    });
    self.pathFailedId = ko.observable(node.PathFailedId);
    self.rebootCount = ko.observable(node.RebootCount);
    self.children = ko.observable(node.Children);
    self.nodesHeard = ko.observable(node.NodesHeard);
    self.nodesNoLongerHeard = ko.observable(node.NodesNoLongerHeard);
    self.nodesHeardBy = ko.observable(node.NodesHeardBy);
    self.nodesNoLongerHeardBy = ko.observable(node.NodesNoLongerHeardBy);
    self.maxHeard = ko.observable(node.MaxHeard);  
    self.maxHeardBy = ko.observable(node.MaxHeardBy);
    self.msgsRxed = ko.observable(node.MsgsRxed);
    self.lastSensorValue = ko.observable(node.LastSensorValue);
    self.lastVoltageValue = ko.observable(node.LastVoltageValue);
    self.lastMsgTime = ko.observable(node.LastMsgTime);
    self.lastRouteUpdateTime = ko.observable(node.LastRouteUpdateTime);
    self.lastStateChangeTime = ko.observable(node.LastStateChangeTime);
    self.lastBootedTime = ko.observable(node.LastBootedTime);
    
    self.isParentHighlight = ko.observable(false);
    self.nodeBeingViewed = ko.observable(false);
    self.status = ko.computed(function() {
        var curStateBin = self.currentState().toString(2).split('').reverse().join('');
        if(curStateBin.charAt(9) == '1' 
            || curStateBin.charAt(8) == '1'
            || curStateBin.charAt(7) == '1'
            || curStateBin.charAt(5) == '1'
            || curStateBin.charAt(4) == '1'
            || curStateBin.charAt(1) == '1')
            return "error";
        else if(curStateBin.charAt(10) == '1'
                || curStateBin.charAt(6) == '1'
                || curStateBin.charAt(3) == '1'
                || curStateBin.charAt(2) == '1'
                || curStateBin.charAt(0) == '1')
            return "warning";
        else
            return "good";
        /*switch(self.currentState()) {
            case 0:
            case 4:
                return "good";
            case 8:
            case 64:
            case 1024:
                return "warning";
            case 1:
            case 2:
            case 16:
            case 32:
            case 128:
            case 256:
            case 512:
                return "error";
            default:
                return "warning";
            }*/
    });
    
    self.stateCss = ko.computed(function() {
        var css = "";
        if(self.status() === "good")
            css = "node-good";
        else if(self.status() === "warning")
            css = "node-warn";
        else if(self.status() === "error")
            css = "node-error";
        return css;
    });
}

function NodeListViewModel() {
    var self = this;
    self.nodes = ko.observableArray([]);
    self.isLoading = ko.observable(true);
    
    self.distinctNodes = ko.computed(function(){
        var distinct = _.uniq(self.nodes(), 'nodeId' );
        return distinct;
    }, self);
    
    self.addNodes = function(nodes) {
        _.each(nodes, function(node) {
            var index = _.findIndex(self.nodes(), function(val) { return val.nodeId == node.nodeId; });
            if(index > 0)
                self.nodes.splice(index, 0, node);
            else
                self.nodes.push(node);
        });
    }
    
    self.isNotFunction = function(obj, key) {
        if(key == 'stateCss' || key == 'hierarchyLevel')
            return false;
        return true;
    }
    
    self.stateStringCss = function(node, key) {
        if(key !== 'currentStateString')
            return '';
        else
            return node.stateCss();
    }

    self.distinctNodesHierarchy = ko.computed(function(){
        var hierarchy = _.sortBy(self.distinctNodes(), function(node){ return node.parentId(); });
        var sinkNode = _.find(hierarchy, function(node) { return node.isSink() });
        hierarchy = _.each(hierarchy, function(node) {
            if(node.nodeId == sinkNode.nodeId)
                node.hierarchyLevel = 0;
            else {
                var level = 1;
                var curNode = node;
                while(curNode.parentId() != sinkNode.nodeId) {
                    level++;
                    curNode = _.find(self.distinctNodes(), function(iNode){return iNode.nodeId == curNode.parentId()});
                }
                node.hierarchyLevel = level;
            }
        });
        //console.log(hierarchy);
        return hierarchy;
    }, self);
    
    self.shouldGoToNextLevel = function(node, index) {
        if(index+1 < self.distinctNodesHierarchy().length && self.distinctNodesHierarchy()[index+1].hierarchyLevel > node.hierarchyLevel)
            return true;
        else
            return false;
    }
    
    self.toggleParentHighlight = function(node, event) {
        var pNode = _.findWhere(self.distinctNodesHierarchy(), { nodeId: node.parentId() } );
        var canvas = $("#canvas" + node.nodeId)[0];
        var canvasObj = $("#canvas" + node.nodeId);
        var context = canvas.getContext('2d');
        if(!pNode.isParentHighlight()) {
            var parentObj = $("#nodeCircle" + node.parentId());
            var nodeObj = $("#nodeCircle" + node.nodeId);
            var left = Math.min(parentObj.offset().left, nodeObj.offset().left) + 75;
            canvasObj.css({top: parentObj.offset().top + 75, left: left});
            var height = Math.max(1,Math.abs(nodeObj.offset().top - parentObj.offset().top));
            var width = Math.max(1, Math.abs(nodeObj.offset().left - parentObj.offset().left));
            canvasObj.height(height);
            canvasObj.width(width);
            canvas.height = height;
            canvas.width = width;
            context.beginPath();
            if(parentObj.offset().left <= nodeObj.offset().left) {
                context.moveTo(0, 0);
                context.lineTo(width, height);
            }
            else {
                context.moveTo(width, 0);
                context.lineTo(0, height);
            }
            context.stroke();
            $("#canvas" + node.nodeId).show();
        }
        else{
            context.clearRect(0,0, canvas.width, canvas.height); 
            $("#canvas" + node.nodeId).hide();
        }
        pNode.isParentHighlight(!pNode.isParentHighlight());
    }
    
    self.rawDataModalVisible = ko.observable(false);
    self.historyModalVisible = ko.observable(false);
    
    self.rawDataModalVisible.subscribe(function(newVal) {
        self.togglePopover(newVal);
    });
    
    self.historyModalVisible.subscribe(function(newVal) {
        self.togglePopover(newVal);
    });
    
    self.togglePopover = function(newVal) {
        var popover = $(".popover");
        if(newVal)
            popover.hide();
        else
            popover.show();
        
    }
    
    self.modalHeaderLabel = ko.observable("");
    self.selectedNode = ko.observable({});
    self.rawDataMessage = ko.observable({});
    self.fullHistoryData = ko.observable({});
    self.isLoadingFullHistory = ko.observable(false);
    
    self.showRawData = function(nodeId) {
        var node = _.findWhere(self.distinctNodesHierarchy(), {nodeId: nodeId});
        self.rawDataMessage(node);
        self.modalHeaderLabel("Raw Message #" + node.id);
        self.rawDataModalVisible(true);
    }
    
    self.showFullHistory = function(nodeId) {
        self.isLoadingFullHistory(true);
        self.historyModalVisible(true);
        self.modalHeaderLabel("Message History for Node " + nodeId);
        $.ajax({
                url: 'http://127.0.0.1:9250/getHistory',
                type: 'GET',
                dataType: 'json',
                data: {nodeId: nodeId},
                success: function(data) {
                    self.fullHistoryData($.map(data, function(node) { return new Node(node) }));
                    self.isLoadingFullHistory(false);
                },
                error: function(err) { alert(err.responseText); }
        });
    }
    
    self.viewNode = ko.observable(self.nodes()[0]);
    self.changeSelectedMessage = function(node) {
        if(self.viewNode())
            self.viewNode().nodeBeingViewed(false);
        self.viewNode(node);
        self.viewNode().nodeBeingViewed(true);
    }
    
    self.latestTimeReceived = ko.observable(0);
    self.updateData = function() {
        $.ajax({
            url: 'http://127.0.0.1:9250/update',
            type: 'GET',
            dataType: 'json',
            data: {maxTime: self.latestTimeReceived()},
            success: function(data) {
                self.addNodes($.map(data, function(node) { return new Node(node) }));
                self.latestTimeReceived(self.getLatestTime(self.nodes()));
            },
            error: function(err) {
                alert(err.responseText);
            },
            complete: function() {
                setTimeout(self.updateData, 5000);
            }
        });
    }
    
    self.getLatestTime = function(nodes) {
        var max = 0;
        _.each(nodes, function(node) {
            if(node.currentDateVal > max)
                max = node.currentDateVal;
        });
        return max;
    }
    
    $.ajax({
            url: 'http://127.0.0.1:9250/getAll',
            type: 'GET',
            dataType: 'json',
            success: function(data) {
                self.nodes($.map(data, function(node) { return new Node(node) }));
                self.latestTimeReceived(self.getLatestTime(self.nodes()));
                setTimeout(self.updateData, 5000);
                self.isLoading(false);
            },
            error: function(err) { alert(err.responseText); }
    });
}

//-----------------------Spinner----------------------------------
ko.bindingHandlers.spinner = {
    init: function(element, valueAccessor, allBindingsAccessor) {
        var options = {};
        $.extend(options, ko.bindingHandlers.spinner.defaultOptions, ko.utils.unwrapObservable(allBindingsAccessor().spinnerOptions));
        
        //create the spinner with the appropriate options
        var spinner = new Spinner(options);
        
        //could do this in the update function, but this gives us easy access to the spinner object (through the closure) without having to store it with the element
        ko.dependentObservable(function() {
            var value = ko.utils.unwrapObservable(valueAccessor());  //this ensures that it depends on the observable value
            if (value) {
               spinner.spin(element);    
            } else if (spinner.el) {   //don't stop first time
                spinner.stop(); 
            }
        });     
    },
    defaultOptions: {
        lines: 10,
        length: 3,
        width: 3,
        radius: 5,
        color: '#000',
        speed: 1,
        trail: 75,
        shadow: true
    }
};

ko.applyBindings(new NodeListViewModel());