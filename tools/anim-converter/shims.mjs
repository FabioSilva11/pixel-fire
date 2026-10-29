// O FBXLoader do three.js foi feito para o navegador: estes substitutos mínimos deixam ler os FBX no Node
// (texturas embutidas são ignoradas; só esqueleto e animação interessam ao conversor).
globalThis.self = globalThis;
globalThis.window = globalThis;
class FakeImage { set src(_) { setTimeout(() => this.onload && this.onload(), 0); } addEventListener(t, f) { if (t === 'load') this.onload = f; } removeEventListener() {} }
globalThis.Image = FakeImage;
globalThis.document = {
  createElementNS: () => new FakeImage(),
  createElement: () => ({ getContext: () => null, style: {} }),
};
globalThis.URL.createObjectURL = globalThis.URL.createObjectURL || (() => 'blob:');
