import LibRawModule from './libraw.js';

let _modulePromise = null;

/**
 * Returns a Promise that resolves to the loaded Emscripten module.
 * We only load the module once and cache it in _modulePromise.
 */
async function getLibRawModule() {
	if (!_modulePromise) {
		_modulePromise = LibRawModule(); // This returns a promise (MODULARIZE=1)
	}
	return _modulePromise;
}

export default class LibRaw {
	constructor() {
		// On construction, wait until the module is ready, then instantiate a new C++ LibRaw object.
		this._ready = getLibRawModule().then((mod) => {
			this._libraw = new mod.LibRaw();
		});
	}

	/**
	 * Open/parse the RAW data with optional settings
	 */
	async open(buffer, settings) {
		await this._ready;
		// Here, each JS LibRaw instance calls its own underlying C++ object
		return this._libraw.open(buffer, settings);
	}

	/**
	 * Retrieve metadata
	 */
	async metadata(fullOutput) {
		await this._ready;
		let metadata = this._libraw.metadata(!!fullOutput);
		// Example: convert numeric thumb_format to a string
		if (metadata?.hasOwnProperty('thumb_format')) {
			metadata.thumb_format = [
				'unknown',
				'jpeg',
				'bitmap',
				'bitmap16',
				'layer',
				'rollei',
				'h265'
			][metadata.thumb_format] || 'unknown';
		}
		// Trim desc if present
		if (metadata?.hasOwnProperty('desc')) {
			metadata.desc = String(metadata.desc).trim();
		}
		if (metadata?.hasOwnProperty('timestamp')) {
			metadata.timestamp = new Date(metadata.timestamp);
		}
		return metadata;
	}

	/**
	 * Retrieve processed image data (synchronously from the perspective of C++,
	 * but we've already awaited the module & instance.)
	 */
	async imageData() {
		await this._ready;
		return this._libraw.imageData();
	}
}