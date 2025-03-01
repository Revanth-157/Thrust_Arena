const express = require('express');
const WebSocket = require('ws');  
const app = express();  
const server = require('http').createServer(app);  
const wss = new WebSocket.Server({ server });  

const port = 3000;

app.use(express.static('frontend'));

server.listen(port, () => {

    console.log(`Server running at http://localhost:${port}`);
});
