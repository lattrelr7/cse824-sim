---------------------------------
-- Table '824Sim'.'NodeModel'
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS NodeModel (
  'Id' INTEGER PRIMARY KEY AUTOINCREMENT,
  'CurrentTime' INT,
  'NodeId' INT,
  'ParentId' INT,
  'IsSink' INT,
  'CurrentState' INT,
  'PathFailedId' INT,
  'Children' TEXT,
  'NodesHeard' TEXT,
  'NodesNoLongerHeard' TEXT,
  'NodesHeardBy' TEXT,
  'NodesNoLongerHeardBy' TEXT,
  'MaxHeard' INT,
  'MaxHeardBy' INT,
  'MsgsRxed' INT,
  'LastSensorValue' INT,
  'LastVoltageValue' INT,
  'LastMsgTime' INT,
  'LastRouteUpdateTime' INT,
  'LastStateChangeTime' INT
  );
  

-- -----------------------------------------------------
-- Table '824Sim'.'FaultMessages'
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS FaultMessages (
  'Id' INTEGER PRIMARY KEY AUTOINCREMENT,
  'NodeId' INT,
  'Value' INT
  );
