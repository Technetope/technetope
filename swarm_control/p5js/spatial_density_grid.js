// 多層グリッドベースの密度計算（デッドロック未然防止）

class SpatialDensityGrid {
    constructor() {
        // 3レイヤーのグリッドサイズ
        this.layers = [
            { cellSize: 50.0, weight: 1.0, name: 'near' },      // レイヤー1（近距離）：5cm×5cm
            { cellSize: 100.0, weight: 0.6, name: 'medium' }, // レイヤー2（中距離）：10cm×10cm
            { cellSize: 200.0, weight: 0.3, name: 'far' }     // レイヤー3（遠距離）：20cm×20cm
        ];
        
        // 各レイヤーのグリッド
        this.grids = [];
        this.gridWidths = [];
        this.gridHeights = [];
        
        // フィールドサイズ
        this.fieldMinX = Params.field.minX;
        this.fieldMinY = Params.field.minY;
        this.fieldMaxX = Params.field.maxX;
        this.fieldMaxY = Params.field.maxY;
        this.fieldWidth = this.fieldMaxX - this.fieldMinX;
        this.fieldHeight = this.fieldMaxY - this.fieldMinY;
        
        // 各レイヤーのグリッドを初期化
        this.initialize();
    }
    
    // グリッドを初期化
    initialize() {
        this.grids = [];
        this.gridWidths = [];
        this.gridHeights = [];
        
        for (const layer of this.layers) {
            const cellSize = layer.cellSize;
            const gridWidth = Math.ceil(this.fieldWidth / cellSize) + 1;
            const gridHeight = Math.ceil(this.fieldHeight / cellSize) + 1;
            
            // グリッドを初期化（各セルの混雑度を0に）
            const grid = [];
            for (let y = 0; y < gridHeight; y++) {
                const row = [];
                for (let x = 0; x < gridWidth; x++) {
                    row.push(0); // 混雑度（ロボット数）
                }
                grid.push(row);
            }
            
            this.grids.push(grid);
            this.gridWidths.push(gridWidth);
            this.gridHeights.push(gridHeight);
        }
    }
    
    // ロボット位置からグリッドセルを更新
    update(robots) {
        // すべてのグリッドをリセット
        for (let layerIdx = 0; layerIdx < this.layers.length; layerIdx++) {
            const grid = this.grids[layerIdx];
            for (let y = 0; y < this.gridHeights[layerIdx]; y++) {
                for (let x = 0; x < this.gridWidths[layerIdx]; x++) {
                    grid[y][x] = 0;
                }
            }
        }
        
        // 各ロボットの位置をグリッドに追加
        for (const robot of robots) {
            for (let layerIdx = 0; layerIdx < this.layers.length; layerIdx++) {
                const cellSize = this.layers[layerIdx].cellSize;
                const gridX = Math.floor((robot.x - this.fieldMinX) / cellSize);
                const gridY = Math.floor((robot.y - this.fieldMinY) / cellSize);
                
                // グリッド範囲内かチェック
                if (gridX >= 0 && gridX < this.gridWidths[layerIdx] &&
                    gridY >= 0 && gridY < this.gridHeights[layerIdx]) {
                    this.grids[layerIdx][gridY][gridX]++;
                }
            }
        }
    }
    
    // 位置からグリッドセルの座標を取得
    getGridCell(x, y, layerIdx) {
        const cellSize = this.layers[layerIdx].cellSize;
        const gridX = Math.floor((x - this.fieldMinX) / cellSize);
        const gridY = Math.floor((y - this.fieldMinY) / cellSize);
        
        return { gridX, gridY };
    }
    
    // 位置の混雑度を取得（3レイヤーを重み付けして合成）
    getDensity(x, y) {
        let totalDensity = 0.0;
        let totalWeight = 0.0;
        
        for (let layerIdx = 0; layerIdx < this.layers.length; layerIdx++) {
            const { gridX, gridY } = this.getGridCell(x, y, layerIdx);
            const layer = this.layers[layerIdx];
            
            // グリッド範囲内かチェック
            if (gridX >= 0 && gridX < this.gridWidths[layerIdx] &&
                gridY >= 0 && gridY < this.gridHeights[layerIdx]) {
                const density = this.grids[layerIdx][gridY][gridX];
                totalDensity += density * layer.weight;
                totalWeight += layer.weight;
            }
        }
        
        // 重みで正規化
        return totalWeight > 0 ? totalDensity / totalWeight : 0.0;
    }
    
    // 8方向の混雑度を取得（各ロボットの周辺領域を判定）
    getDirectionalDensities(x, y, radius = 100.0) {
        const directions = [
            { angle: 0, dx: 1, dy: 0 },      // 右
            { angle: Math.PI / 4, dx: 1, dy: 1 },   // 右上
            { angle: Math.PI / 2, dx: 0, dy: 1 },  // 上
            { angle: 3 * Math.PI / 4, dx: -1, dy: 1 }, // 左上
            { angle: Math.PI, dx: -1, dy: 0 },      // 左
            { angle: 5 * Math.PI / 4, dx: -1, dy: -1 }, // 左下
            { angle: 3 * Math.PI / 2, dx: 0, dy: -1 }, // 下
            { angle: 7 * Math.PI / 4, dx: 1, dy: -1 }  // 右下
        ];
        
        const densities = [];
        
        for (const dir of directions) {
            const sampleX = x + dir.dx * radius;
            const sampleY = y + dir.dy * radius;
            const density = this.getDensity(sampleX, sampleY);
            densities.push({
                angle: dir.angle,
                density: density,
                x: sampleX,
                y: sampleY
            });
        }
        
        return densities;
    }
    
    // 混雑度が最も低い方向を取得
    getLeastCrowdedDirection(x, y, radius = 100.0) {
        const densities = this.getDirectionalDensities(x, y, radius);
        
        // 混雑度が最も低い方向を探す
        let minDensity = Infinity;
        let bestDirection = null;
        
        for (const dir of densities) {
            if (dir.density < minDensity) {
                minDensity = dir.density;
                bestDirection = dir;
            }
        }
        
        return bestDirection;
    }
}

