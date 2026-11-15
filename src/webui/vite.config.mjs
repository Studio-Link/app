import { defineConfig } from "vite";
import { readFileSync, readdirSync } from "node:fs";
import { resolve } from "node:path";
import Handlebars from "handlebars";
import purgecss from "@fullhuman/postcss-purgecss";

const src = resolve(import.meta.dirname, "src");

// Roboto Mono, served locally by the C webapp module (no Google Fonts
// request: works offline and avoids leaking user IPs to third parties).
const fontDir = resolve(
  import.meta.dirname,
  "node_modules/@fontsource/roboto-mono/files",
);
const fontWeights = [400, 500, 700];

// Precompile *.handlebars templates against the shared runtime instance
// (helpers are registered on it in src/app.js).
function handlebarsTemplates() {
  return {
    name: "handlebars-templates",
    transform(code, id) {
      if (!id.endsWith(".handlebars")) return null;
      return {
        code:
          'import Handlebars from "handlebars/runtime";\n' +
          `export default Handlebars.template(${Handlebars.precompile(code)});\n`,
        map: null,
      };
    },
  };
}

// Emit index.html unmodified, the images and fonts, as expected by the C
// webapp module (see make.sh / dist/lib/functions.sh).
function staticFiles() {
  return {
    name: "static-files",
    buildStart() {
      this.addWatchFile(resolve(src, "index.html"));
    },
    generateBundle() {
      this.emitFile({
        type: "asset",
        fileName: "index.html",
        source: readFileSync(resolve(src, "index.html")),
      });
      for (const f of readdirSync(resolve(src, "images"))) {
        this.emitFile({
          type: "asset",
          fileName: `images/${f}`,
          source: readFileSync(resolve(src, "images", f)),
        });
      }
      for (const w of fontWeights) {
        this.emitFile({
          type: "asset",
          fileName: `fonts/roboto-mono-${w}.woff2`,
          source: readFileSync(
            resolve(fontDir, `roboto-mono-latin-${w}-normal.woff2`),
          ),
        });
      }
    },
  };
}

export default defineConfig(({ mode }) => {
  const production = mode === "production";

  return {
    publicDir: false,
    plugins: [handlebarsTemplates(), staticFiles()],
    css: {
      preprocessorOptions: {
        scss: {
          quietDeps: true,
          silenceDeprecations: ["import", "global-builtin", "color-functions"],
        },
      },
      postcss: {
        plugins: production
          ? [
              purgecss({
                content: [
                  "src/index.html",
                  "src/**/*.js",
                  "src/**/*.handlebars",
                  "node_modules/bootstrap/dist/js/bootstrap.esm.js",
                  "node_modules/driver.js/dist/*.js",
                ],
                defaultExtractor: (content) =>
                  content.match(/[\w-/.:]+(?<!:)/g) || [],
                safelist: {
                  greedy: [/-active$/, /-enter$/, /-leave-to$/, /show$/, /data-bs-popper/],
                },
              }),
            ]
          : [],
      },
    },
    build: {
      outDir: "dist",
      emptyOutDir: true,
      minify: production,
      sourcemap: false,
      cssCodeSplit: false,
      rollupOptions: {
        input: resolve(src, "main.js"),
        output: {
          format: "iife",
          entryFileNames: "app.js",
          assetFileNames: (asset) =>
            asset.names?.some((n) => n.endsWith(".css"))
              ? "app.css"
              : "assets/[name][extname]",
        },
      },
    },
  };
});
