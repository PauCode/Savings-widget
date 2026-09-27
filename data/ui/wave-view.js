class WaveView {
    constructor(canvas) {
        this.canvas = canvas;
        this.context = canvas.getContext("2d");
        this.canvas.hidden = false;
        document.body.dataset.jarMode = "liquid";

        this.width = 0;
        this.canvasHeight = 0;
        this.height = 0;
        this.progress = 0;
        this.targetProgress = 0;
        this.progressVelocity = 0;
        this.phase = 0;
        this.waveImpulse = 0;
        this.frameRequest = 0;
        this.lastFrameTime = 0;
        this.lastDrawTime = 0;
        this.refreshSamples = [];
        this.estimatedRefreshRate = 60;
        this.maxFrameRate = 30;

        this.resizeObserver = new ResizeObserver(() => this.resize());
        this.resizeObserver.observe(canvas.parentElement);
        this.resize();
    }

    setProgress(progressRatio) {
        this.targetProgress = Math.max(0, Math.min(100, Number(progressRatio) || 0));
        this.scheduleFrame();
    }

    resize() {
        const bounds = this.canvas.getBoundingClientRect();
        if (!bounds.width || !bounds.height) {
            return;
        }

        const scale = Math.min(window.devicePixelRatio || 1, 2);
        this.width = bounds.width;
        this.canvasHeight = bounds.height;
        this.height = bounds.height;
        this.canvas.width = Math.round(bounds.width * scale);
        this.canvas.height = Math.round(bounds.height * scale);
        this.context.setTransform(scale, 0, 0, scale, 0, 0);
        this.draw();
    }

    scheduleFrame() {
        if (!this.frameRequest) {
            this.frameRequest = requestAnimationFrame((time) => this.animate(time));
        }
    }

    animate(time) {
        this.frameRequest = 0;
        if (this.lastFrameTime > 0) {
            const interval = time - this.lastFrameTime;
            if (interval >= 4 && interval <= 80) {
                this.refreshSamples.push(interval);
                if (this.refreshSamples.length > 24) {
                    this.refreshSamples.shift();
                }
                if (this.refreshSamples.length >= 8) {
                    const sorted = [...this.refreshSamples].sort((left, right) => left - right);
                    this.estimatedRefreshRate = 1000 /
                        sorted[Math.floor(sorted.length / 2)];
                }
            }
        }

        const frameRate = Math.min(this.maxFrameRate, this.estimatedRefreshRate);
        const minimumInterval = 1000 / Math.max(1, frameRate);
        if (this.lastDrawTime > 0 &&
            time - this.lastDrawTime < minimumInterval - 0.5) {
            this.lastFrameTime = time;
            this.scheduleFrame();
            return;
        }

        const delta = this.lastFrameTime > 0
            ? Math.min((time - this.lastFrameTime) / 1000, 0.05)
            : 1 / frameRate;
        this.lastFrameTime = time;
        this.lastDrawTime = time;
        this.phase += delta * 3.4;
        this.waveImpulse *= Math.exp(-5.5 * delta);

        const acceleration = (this.targetProgress - this.progress) * 95;
        this.progressVelocity = (this.progressVelocity + acceleration * delta) *
            Math.exp(-11 * delta);
        this.progress += this.progressVelocity * delta;
        if (Math.abs(this.targetProgress - this.progress) < 0.02 &&
            Math.abs(this.progressVelocity) < 0.1) {
            this.progress = this.targetProgress;
            this.progressVelocity = 0;
        }

        this.draw();
        this.scheduleFrame();
    }

    draw() {
        if (!this.context || !this.width || !this.height) {
            return;
        }

        const context = this.context;
        const left = 4;
        const right = this.width - 4;
        const top = 0;
        const bottom = top + this.height;
        const upperRadius = 25;
        const lowerRadius = 41;
        const progress = Math.max(0, Math.min(100, this.progress));
        const surfaceY = bottom - (bottom - top) * progress / 100;
        const style = getComputedStyle(this.canvas);
        const liquidTop = style.getPropertyValue("--liquid-top").trim() || "#1ed3a0";
        const liquidBottom = style.getPropertyValue("--liquid-bottom").trim() || "#1685a2";
        const highlight = style.getPropertyValue("--liquid-highlight").trim() || "#b5ffe9";

        context.clearRect(0, 0, this.width, this.canvasHeight);
        if (progress <= 0) {
            return;
        }

        context.save();
        context.beginPath();
        context.moveTo(left + upperRadius, top);
        context.lineTo(right - upperRadius, top);
        context.quadraticCurveTo(right, top, right, top + upperRadius);
        context.lineTo(right, bottom - lowerRadius);
        context.quadraticCurveTo(right, bottom, right - lowerRadius, bottom);
        context.lineTo(left + lowerRadius, bottom);
        context.quadraticCurveTo(left, bottom, left, bottom - lowerRadius);
        context.lineTo(left, top + upperRadius);
        context.quadraticCurveTo(left, top, left + upperRadius, top);
        context.closePath();
        context.clip();

        context.beginPath();
        const amplitude = Math.min(5, this.height * 0.024) +
            Math.min(3, Math.abs(this.waveImpulse));
        for (let x = left; x <= right; x += 3) {
            const ratio = (x - left) / (right - left);
            const wave = Math.sin(ratio * Math.PI * 2.2 + this.phase) * amplitude;
            const impulse = Math.exp(-Math.abs(ratio - 0.55) * 8) * this.waveImpulse;
            const y = surfaceY + wave + impulse;
            if (x === left) {
                context.moveTo(x, y);
            } else {
                context.lineTo(x, y);
            }
        }
        context.lineTo(right, bottom);
        context.lineTo(left, bottom);
        context.closePath();

        const gradient = context.createLinearGradient(0, surfaceY, 0, bottom);
        gradient.addColorStop(0, liquidTop);
        gradient.addColorStop(0.58, liquidBottom);
        gradient.addColorStop(1, "#154864");
        context.fillStyle = gradient;
        context.fill();

        context.beginPath();
        for (let x = left; x <= right; x += 3) {
            const ratio = (x - left) / (right - left);
            const wave = Math.sin(ratio * Math.PI * 2.2 + this.phase) * amplitude;
            const impulse = Math.exp(-Math.abs(ratio - 0.55) * 8) * this.waveImpulse;
            const y = surfaceY + wave + impulse;
            if (x === left) {
                context.moveTo(x, y);
            } else {
                context.lineTo(x, y);
            }
        }
        context.strokeStyle = highlight;
        context.globalAlpha = 0.72;
        context.lineWidth = 1.5;
        context.stroke();
        context.restore();
    }
}