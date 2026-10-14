# Publishing the website on GitHub pages

These files live in `docs/web/` of github.com/razobill/razobill. The workflow `.github/workflows/pages.yml` deploys that folder.

1. Copy the contents of this package into the repo (`docs/web/*` and `.github/workflows/pages.yml`), commit, push to `main`.
2. Settings > Pages > Source: **GitHub Actions**. Live in about a minute at <https://razobill.github.io/razobill/>
3. Check: /llms.txt, /robots.txt and /sitemap.xml load. Paste the URL into search.google.com/test/rich-results (expect FAQ and Software data).
4. Repo "About" gear: set the website to the Pages URL; add topics: log-parsing, observability, duckdb, simd, cpp20, lossless-compression, drain3, parquet.
5. Google Search Console and Bing Webmaster Tools: add the URL prefix, submit sitemap.xml, request indexing of the home page.
6. After content changes, update `<lastmod>` in sitemap.xml and `dateModified` in index.html.

Using a custom domain? Replace `https://razobill.github.io/razobill/` in index.html, robots.txt, sitemap.xml and llms.txt, and change the `/razobill/` paths in 404.html.
