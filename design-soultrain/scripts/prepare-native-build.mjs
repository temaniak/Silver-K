import { rm, readdir } from 'node:fs/promises';
// Reference screenshots belong to the design workspace, not the plugin bundle.
for (const name of ['reference.png','figma-reference.png','figma-options-reference.png']) await rm(new URL(`../dist/client/${name}`,import.meta.url),{force:true});
const fonts=new URL('../dist/client/fonts/static/',import.meta.url);
for (const name of await readdir(fonts)) if(!['Inter-Regular.ttf','Inter-Medium.ttf','Inter-SemiBold.ttf'].includes(name)) await rm(new URL(name,fonts));
