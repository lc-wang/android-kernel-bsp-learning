/**
 * generate_index.js
 * - 掃描 repo 的 notes / investigation / code / examples 目錄
 * - 產生 assets/notes_index.json（包含 folder/subfolder -> [ { name, path, raw_url } ])
 *
 * 執行於 GitHub Actions（使用 local file system），不用 API，所以不會有 rate-limit 問題。
 */

const fs = require('fs');
const path = require('path');

const TOP_DIRS = ['notes','cases-study'];
const OUT_DIR = 'assets';
const OUT_FILE = path.join(OUT_DIR, 'notes_index.json');

function isDir(p) {
  try { return fs.statSync(p).isDirectory(); } catch(e){ return false; }
}

function isFile(p) {
  try { return fs.statSync(p).isFile(); } catch(e){ return false; }
}

// get raw.githubusercontent.com base URL for this repo on current branch
function rawBaseUrl(branch) {
  // NOTE: Actions checks out repository at HEAD, we can detect branch via GITHUB_REF
  const owner = process.env.GITHUB_REPOSITORY && process.env.GITHUB_REPOSITORY.split('/')[0];
  const repo  = process.env.GITHUB_REPOSITORY && process.env.GITHUB_REPOSITORY.split('/')[1];
  const ref = process.env.GITHUB_REF ? process.env.GITHUB_REF.replace('refs/heads/','') : 'main';
  return `https://raw.githubusercontent.com/${owner}/${repo}/${ref}`;
}

function buildIndex() {
  const repoRoot = process.cwd();
  const baseRaw = rawBaseUrl();
  const data = {};

  TOP_DIRS.forEach(d => {
    const dirPath = path.join(repoRoot, d);
    if (!isDir(dirPath)) return;
    data[d] = [];

    // if notes -> expect subdirs (android/bsp/kernel)
    if (d === 'notes') {
      const subs = fs.readdirSync(dirPath).filter(s=> isDir(path.join(dirPath,s)));
      subs.forEach(sub => {
        const subPath = path.join(dirPath, sub);
        const files = fs.readdirSync(subPath).filter(f=> isFile(path.join(subPath,f)));
        const items = files.map(f => {
          return {
            name: f,
            path: `${d}/${sub}/${f}`,
            raw_url: `${baseRaw}/${d}/${sub}/${f}`
          };
        });
        data[d].push({ subfolder: sub, files: items });
      });
    } else {
      // for other dirs: list files and subdirectories
      const items = fs.readdirSync(dirPath);
      items.forEach(it => {
        const itPath = path.join(dirPath, it);
        if (isFile(itPath)) {
          data[d].push({
            type: 'file',
            name: it,
            path: `${d}/${it}`,
            raw_url: `${baseRaw}/${d}/${it}`
          });
        } else if (isDir(itPath)) {
          const files = fs.readdirSync(itPath).filter(f=> isFile(path.join(itPath,f)));
          const subfiles = files.map(f => ({
            name: f,
            path: `${d}/${it}/${f}`,
            raw_url: `${baseRaw}/${d}/${it}/${f}`
          }));
          data[d].push({
            type: 'subdir',
            name: it,
            files: subfiles
          });
        }
      });
    }
  });

  // ensure assets folder exists
  if (!fs.existsSync(OUT_DIR)) fs.mkdirSync(OUT_DIR, { recursive: true });

  fs.writeFileSync(OUT_FILE, JSON.stringify({ generated_at: new Date().toISOString(), index: data }, null, 2), 'utf8');
  console.log('Wrote', OUT_FILE);
}

buildIndex();
