# Firebase Configuration 

1. **Database Rules**:  
```json
{
  "rules": {
    "sensorData": { ".read": true, ".write": false },
    "relayControl": { ".read": true, ".write": true }
  }
}
