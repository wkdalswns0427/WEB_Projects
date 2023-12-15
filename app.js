require('dotenv').config();
const path = require('path');
const express = require('express');
const mongoose = require('mongoose');
// const { PORT, MONGO_URI } = process.env;

const app = express();
const PORT = 6060;

app.use(express.static(path.join(__dirname,'public')));
app.use(express.static('public'));
app.use(express.urlencoded({extended : true}));
app.use(express.json());



app.get('/', function(req, res){
    res.sendFile(__dirname + '/page.html');
});

app.listen(PORT, () => console.log(`Server listening on port 6060`));
// var server = app.listen(5555, function(){
//     console.log('loaded');
// });