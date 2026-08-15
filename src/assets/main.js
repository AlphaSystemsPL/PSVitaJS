
let inc = 0;

// let font = Font.load_font_file("app0:/assets/segoeui.ttf");

const texture1 = Screen.load_png_file("app0:/assets/test1.png");

let pads = Pads;
let x = 200, y = 200;
let steps = 3;

const response = Net.get(
    "http://google.com"
);

console.log("status:", response.status);
console.log("ok:", response.ok);

// Basically creates an infinite loop, similar to while true (you can use it too).
let interval = os.setInterval(() => {

    if (pads.check(pads.UP))
        y -= steps;

    if (pads.check(pads.DOWN))
        y += steps;

    if (pads.check(pads.LEFT))
        x -= steps;

    if (pads.check(pads.RIGHT))
        x += steps;

    Screen.start_drawing()
    Screen.clear(0, 50, 50, 0) // rgba

    //console.log(font)

    //font.print(font, `Hello world menó! inc = ${inc++}`, 50, 50, 20) // font, str, x, y, sz, r, g, b, a.
    // ^ dando erro na linha 30. verificar se new_font (font.c) retorna a fonteId corretamente.

    Screen.draw_texture(texture1, x, y)

    Screen.end_drawing()
    Screen.swap_buffers()

    if (pads.check(pads.START) || pads.check(pads.POWER)) {
        console.log('Closing app...\n'); // show the message on stdout.

        os.clearInterval(interval); // it closes the app.

        System.exit();
        return;
    }
}, 0);


const SAVE_DIR = "ux0:/data/VITAJS001";
const SAVE_FILE = `${SAVE_DIR}/save.json`;

os.mkdir(SAVE_DIR);

function saveGame(state) {
    const file = std.open(SAVE_FILE, "w");

    if (!file) {
        throw new Error("Cannot open save file");
    }

    file.puts(JSON.stringify(state));
    file.flush();
    file.close();
}

saveGame({
    level: 4,
    score: 12500,
    player: {
        x: 350,
        y: 180
    },
    savedAt: Date.now()
});

function loadGame() {
    const data = std.loadFile(SAVE_FILE);

    if (data === null) {
        return null;
    }

    return JSON.parse(data);
}

const state = loadGame();

if (state) {
    console.log("Score:", state.score);
}