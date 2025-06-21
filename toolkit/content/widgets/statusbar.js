/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  class MozStatusbar extends MozXULElement {
    renderedOnce = false;

    static get resizerFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <statusbarpanel class="statusbar-resizerpanel">
        <resizer dir="bottomend"/>
      </statusbarpanel>
    `),
        true
      );
      Object.defineProperty(this, "resizerFragment", { value: frag });
      return frag;
    }

    connectedCallback() {
      if (this.renderedOnce) {
        return;
      }
      this.renderedOnce = true;
      this.append(this.constructor.resizerFragment.cloneNode(true));
    }
  }

  customElements.define("statusbar", MozStatusbar);

  class MozStatusbarPanel extends MozXULElement {
    renderedOnce = false;

    static get inheritedAttributes() {
      return {
        ".statusbarpanel-text": "value=label,crop",
        ".statusbarpanel-icon": "src,src=image",
      };
    }

    static get labelFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <label class="statusbarpanel-text" crop="right" flex="1"/>
    `),
        true
      );
      Object.defineProperty(this, "labelFragment", { value: frag });
      return frag;
    }

    static get iconFragment() {
      let frag = document.importNode(
        MozXULElement.parseXULToFragment(`
      <image class="statusbarpanel-icon"/>
    `),
        true
      );
      Object.defineProperty(this, "iconFragment", { value: frag });
      return frag;
    }

    get isIconic() {
      return this.classList.contains("statusbarpanel-iconic");
    }

    get isIconicText() {
      return this.classList.contains("statusbarpanel-iconic-text");
    }

    get isMenuIconic() {
      return this.classList.contains("statusbarpanel-menu-iconic");
    }

    connectedCallback() {
      if (this.renderedOnce) {
        return;
      }
      this.renderedOnce = true;

      // Only menu-iconic panels will insert their own markup with children
      if (!this.isMenuIconic && this.children.length > 0) {
        return;
      }

      if (this.isIconic) {
        this.append(this.constructor.iconFragment.cloneNode(true));
      } else if (this.isIconicText) {
        this.append(this.constructor.iconFragment.cloneNode(true));
        this.append(this.constructor.labelFragment.cloneNode(true));
      } else if (this.isMenuIconic) {
        this.prepend(this.constructor.iconFragment.cloneNode(true));
      } else {
        this.append(this.constructor.labelFragment.cloneNode(true));
      }

      this.initializeAttributeInheritance();
    }
  }

  customElements.define("statusbarpanel", MozStatusbarPanel);
}