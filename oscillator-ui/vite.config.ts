import { defineConfig, type PluginOption } from 'vite';
import { NodeGlobalsPolyfillPlugin } from '@esbuild-plugins/node-globals-polyfill';
import { NodeModulesPolyfillPlugin }  from '@esbuild-plugins/node-modules-polyfill';
import rollupNodePolyFill               from 'rollup-plugin-polyfill-node';

export default defineConfig({
  optimizeDeps: {
    esbuildOptions: {
      plugins: [
        NodeGlobalsPolyfillPlugin({ buffer: true, process: true }),
        NodeModulesPolyfillPlugin()
      ]
    }
  },

  resolve: {
    alias: {
      util:   'rollup-plugin-polyfill-node/polyfills/util',
      stream: 'rollup-plugin-polyfill-node/polyfills/stream',
      // …
    }
  },

  build: {
    rollupOptions: {
      plugins: [
        // <–– cast here to silence the overload‐check
        (rollupNodePolyFill() as PluginOption)
      ]
    }
  }
});
