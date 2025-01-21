import LibRawModule from './libraw.js';

let _instancePromise = null;

// Return a Promise that resolves to the LibRaw class
async function getLibRawClass() {
  if (!_instancePromise) {
    _instancePromise = LibRawModule().then((mod) => mod.LibRaw);
  }
  return _instancePromise;
}

export default class LibRaw {
  constructor() {
    // We store the underlying WASMLibRaw instance, which we create asynchronously
    this._ready = getLibRawClass().then((WASMLibRaw) => {
      this._libraw = new WASMLibRaw();
    });
  }

  async open(buffer, settings) {
    await this._ready;
    return this._libraw.open(buffer, settings);
  }

  async metadata() {
    await this._ready;
    return this._libraw.metadata();
  }

  async imageData() {
    await this._ready;
    return this._libraw.imageData();
  }
}
