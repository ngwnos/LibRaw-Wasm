import { build } from 'esbuild';
import { promises as fs } from "fs";


(async () => {
	try {
		await build({
			entryPoints: ['index.js', 'worker.js'], // Entry point of your library
			outdir: 'dist', // Output directory
			bundle: true, // Bundle all files
			minify: true, // Minify the output
			sourcemap: true, // Generate source maps
			format: 'esm', // Output format (ES Module)
		});
		await fs.copyFile('./libraw.wasm', './dist/libraw.wasm');
		console.log('Build successful!');
	} catch (error) {
		console.error('Build failed:', error);
		process.exit(1);
	}
})();