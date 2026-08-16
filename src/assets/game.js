// PSVitaJS - safe/simple game view
// src/assets/game.js
//
// Deliberately uses only APIs already proven by the system screen:
//   - Pads.check()
//   - Screen.clear()
//   - Screen.draw_rectangle()
//   - Font.font_draw_text()
//
// No Pads.analog(), draw_line(), draw_fill_circle(), textures or runtime loading.

const sfx={
	win:Audio.load_wav("app0:/assets/audio.wav"),
	coin:Audio.load_wav("app0:/assets/coin.wav"),
	once:false,
}

export const Game = (() => {
    const WHITE  = 0xffffffff;
    const MUTED  = rgba(165, 175, 190);
    const GREEN  = rgba(90, 220, 135);
    const YELLOW = rgba(255, 205, 80);
    const BLUE   = rgba(90, 155, 255);
    const PANEL  = rgba(28, 34, 44);
    const PLAYER = rgba(245, 245, 245);
    const TARGET = rgba(255, 190, 60);

    const FIELD = {
        left: 60,
        top: 110,
        right: 900,
        bottom: 480
    };

    const targets = [
        { x: 160, y: 170 },
        { x: 760, y: 170 },
        { x: 470, y: 260 },
        { x: 220, y: 390 },
        { x: 720, y: 390 },
        { x: 350, y: 160 },
        { x: 600, y: 350 },
        { x: 120, y: 320 },
        { x: 820, y: 300 },
        { x: 470, y: 420 }
    ];

    let font = null;
    let playerName = "Player";
    let active = false;

    let playerX = 470;
    let playerY = 280;
    let score = 0;
    let won = false;

    let previousCircle = false;
    let previousStart = false;

    function rgba(r, g, b, a = 255) {
        return (
            ((a & 255) << 24) |
            ((b & 255) << 16) |
            ((g & 255) << 8) |
            (r & 255)
        ) >>> 0;
    }

    function text(x, y, size, value, color = WHITE) {
        Font.font_draw_text(
            font,
            x,
            y,
            color,
            size,
            String(value)
        );
    }

    function clamp(value, min, max) {
        return Math.max(min, Math.min(max, value));
    }

    function resetGame() {
        playerX = 470;
        playerY = 280;
        score = 0;
        won = false;
    }

    function syncEdges() {
        previousCircle = Pads.check(Pads.CIRCLE);
        previousStart = Pads.check(Pads.START);
    }

    function justPressedCircle() {
        const current = Pads.check(Pads.CIRCLE);
        const pressed = current && !previousCircle;
        previousCircle = current;
        return pressed;
    }

    function justPressedStart() {
        const current = Pads.check(Pads.START);
        const pressed = current && !previousStart;
        previousStart = current;
        return pressed;
    }

    function enter(options = {}) {
        font = options.font || font;
        playerName = options.playerName || "Player";
        active = true;

        resetGame();
        syncEdges();
    }

    function leave() {
        active = false;
    }

    function update(dt) {
        if (!active) {
            return null;
        }

        if (justPressedCircle()) {
            return { type: "back" };
        }

        if (justPressedStart()) {
            resetGame();
            return { type: "restart" };
        }

        let dx = 0;
        let dy = 0;

        if (Pads.check(Pads.LEFT))  dx -= 1;
        if (Pads.check(Pads.RIGHT)) dx += 1;
        if (Pads.check(Pads.UP))    dy -= 1;
        if (Pads.check(Pads.DOWN))  dy += 1;

        const boost = Pads.check(Pads.CROSS) ? 1.8 : 1.0;
        const speed = 240 * boost;

        playerX += dx * speed * dt;
        playerY += dy * speed * dt;

        playerX = clamp(playerX, FIELD.left + 14, FIELD.right - 14);
        playerY = clamp(playerY, FIELD.top + 14, FIELD.bottom - 14);

        if (!won && score < targets.length) {
            const target = targets[score];

            // Both player and target are 28x28 rectangles.
            if (
                Math.abs(playerX - target.x) < 28 &&
                Math.abs(playerY - target.y) < 28
            ) {
                score++;
				Audio.play(sfx.coin);

                if (score >= targets.length) {
                    won = true;

                    return {
                        type: "win",
                        score
                    };
                }
            }
        }

        return null;
    }

    function render() {
        Screen.clear(12, 15, 20, 255);

        text(40, 48, 32, "VitaJS Mini Game", WHITE);
        text(
            40,
            78,
            19,
            playerName + " - collect all yellow squares",
            MUTED
        );

        text(
            760,
            55,
            26,
            "Score " + score + "/10",
            won ? GREEN : YELLOW
        );

        // Play field.
        Screen.draw_rectangle(
            FIELD.left,
            FIELD.top,
            FIELD.right - FIELD.left,
            FIELD.bottom - FIELD.top,
            PANEL
        );

        // Current target.
        if (!won && score < targets.length) {
            const target = targets[score];

            Screen.draw_rectangle(
                target.x - 14,
                target.y - 14,
                28,
                28,
                TARGET
            );
        }

        // Player.
        Screen.draw_rectangle(
            playerX - 14,
            playerY - 14,
            28,
            28,
            PLAYER
        );

        // Tiny marker on player, still only draw_rectangle().
        Screen.draw_rectangle(
            playerX + 4,
            playerY - 3,
            8,
            6,
            BLUE
        );

        if (won) {
            text(400, 280, 34, "YOU WIN!", GREEN);
			if(!sfx.once){
				sfx.once=true;
				Audio.play(sfx.win);
			}
        }

        text(
            65,
            515,
            18,
            "D-pad: move   X: boost   START: restart   O: back",
            MUTED
        );
    }

    return {
        enter,
        leave,
        update,
        render
    };
})();
