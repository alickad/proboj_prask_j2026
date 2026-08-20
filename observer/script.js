"use strict";
const VALID_TURNS = ['KAMEN', 'PAPIER', 'NOZNICE', 'error'];
let TURN = null;
let SPEED = 1;
let PAUSED = true;
let PLAYER_NAMES = [null, null];
let GAME_TURNS = [];
let FINAL_SCORE = [null, null];
let ERROR_MSG = '';
function init() {
    let fileSelector = document.getElementById('upload_input');
    if (!fileSelector) {
        return;
    }
    fileSelector.addEventListener('change', (event) => {
        console.log('Detected file, upload, running handle_upload()');
        // @ts-ignore
        handle_upload(event?.target?.files[0]);
    });
}
function handle_upload(file) {
    console.log(`File upload: ${file.name}`);
    const reader = new FileReader();
    reader.readAsText(file);
    reader.addEventListener('load', (e) => {
        // @ts-ignore
        loadGameData(e.target.result.split('\n'));
    });
}
function validateTurn(turn) {
    return VALID_TURNS.indexOf(turn) != -1;
}
function parseGameData(data) {
    console.log(`Parsing game data...`);
    if (!Array.isArray(data)) {
        ERROR_MSG = `Error in parseGameData(): data argument must an array.`;
        return null;
    }
    if (data.length < 3) {
        ERROR_MSG = `Game data too short, requires at least 3 lines.`;
        return null;
    }
    // PLAYER NAMES
    PLAYER_NAMES[0] = data[0];
    PLAYER_NAMES[1] = data[1];
    // SCORE
    let score = data[2].split(' ');
    if (score.length != 2) {
        ERROR_MSG = `Invalid score: Line 3 should contain two numbers, scores of players, got ${score}.`;
        return null;
    }
    if (isNaN(Number.parseInt(score[0]))) {
        ERROR_MSG = `Invalid score: Line 3 should contain two numbers, ${score[0]} is not a number.`;
        return null;
    }
    if (isNaN(Number.parseInt(score[1]))) {
        ERROR_MSG = `Invalid score: Line 3 should contain two numbers, ${score[1]} is not a number.`;
        return null;
    }
    FINAL_SCORE = score;
    // TURNS
    GAME_TURNS = [];
    for (let i = 3; i < data.length; i++) {
        let line = data[i].split(" ");
        if (line.length != 2) {
            if (line.length < 2) {
                ERROR_MSG = `Invalid data structure on line ${i}: Too little turns, requires 2, got ${line.length}`;
            }
            else {
                ERROR_MSG = `Invalid data structure on line ${i}: Too many turns, requires 2, got ${line.length}`;
            }
            return null;
        }
        if (!validateTurn(line[0])) {
            ERROR_MSG = `Invalid turn '${line[0]}' on line ${i}`;
            return null;
        }
        if (!validateTurn(line[1])) {
            ERROR_MSG = `Invalid turn '${line[1]}' on line ${i}`;
            return null;
        }
        GAME_TURNS.push(line);
    }
    TURN = 0;
    return true;
}
function loadGameData(data) {
    const result = parseGameData(data);
    if (result == null) {
        alert(`Error parsing game data: ${ERROR_MSG}.`);
    }
}
// console.log(parseGameData(
//     ['player1', 'player2', '0 0', 'KAMEN KAMEN']
// )&&'[DEBUG] Successfully parsed!')
// console.log(`[DEBUG] PLAYER_NAMES=${PLAYER_NAMES} GAME_TURNS=${GAME_TURNS}`)
function play_animation(turns, speed) {
}
function game_controls_action(action) {
    switch (action) {
        case 'pause':
            break;
        default:
            break;
    }
}
