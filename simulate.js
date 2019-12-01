const express = require('express');
const app = express();
const expressWs = require('express-ws')(app);

class Covering {
    constructor(name) {
        this.name = name;
        this.current_position = 0;
        this.target_position = -1;
        this.max_position = 100;
        this.state = 2;
    }

    goTo(targetPosition) {
        this.target_position = targetPosition;
        this.updateState();
    }

    resetUpPosition() {
        this.current_position = 0;
    }

    resetDownPosition() {
        this.max_position = this.current_position;r
    }

    setState(state) {
        this.state = state;
        this.target_position = -1;
    }

    updateState() {
        if(this.target_position != -1) {
            if (this.target_position < this.current_position) {
                this.state = 1;
            } else if (this.target_position > this.current_position) {
                this.state = 0;
            } else {
                this.state = 2;
            }
        }
    }

    update() {
        if(this.state == 0) {
            this.current_position ++;
            this.updateState();
            return true;
        } else if(this.state == 1) {
            this.current_position --;
            this.updateState();
            return true;
        }
    }
}

const coverings = {
    a: new Covering("Cover A"),
    b: new Covering("Cover B")
};

const wsConnections = [];

function sendWsUpdate() {
    wsConnections.forEach(ws => {
        ws.send(JSON.stringify(coverings));
    })
}

setInterval(() => {
    if(Object.values(coverings).reduce((a,b) => a || b.update(), false)) {
        sendWsUpdate();
    }
}, 1000);

app.get('/status', (req, res) => {
    res.send(coverings);
});
app.get('/:pfx/set_target_position', (req, res) => {
    coverings[req.params.pfx].goTo(parseInt(req.query.pos));
    res.end("OK");
});
app.get('/:pfx/set_up_pos', (req, res) => {
    coverings[req.params.pfx].resetUpPosition();
    res.end("OK");
});
app.get('/:pfx/set_down_pos', (req, res) => {
    coverings[req.params.pfx].resetDownPosition();
    res.end("OK");
});
app.get('/:pfx/up', (req, res) => {
    coverings[req.params.pfx].setState(1);
});
app.get('/:pfx/down', (req, res) => {
    coverings[req.params.pfx].setState(0);
});
app.get('/:pfx/stop', (req, res) => {
    coverings[req.params.pfx].setState(1);
});
app.ws('/status_ws', (ws, req) => {
    wsConnections.push(ws);
    ws.on('close', () => {
        wsConnections.splice(wsConnections.indexOf(ws), 1)
    });
});
app.use(express.static('html'));

app.listen(3000, () => console.log(`listening`));