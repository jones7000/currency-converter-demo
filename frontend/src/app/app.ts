import { Component } from '@angular/core';
import { MatToolbarModule } from '@angular/material/toolbar';

import { Conversion } from './features/conversion/conversion';
import { CurrencyOverview } from './features/currency-overview/currency-overview';

/**
 * Root shell: a header plus the two main sections (currency overview and
 * conversion). No router -- a single page with two independent sections
 * doesn't need navigation state, and adding one would be unjustified
 * complexity.
 */
@Component({
  selector: 'app-root',
  imports: [MatToolbarModule, CurrencyOverview, Conversion],
  templateUrl: './app.html',
  styleUrl: './app.scss',
})
export class App {}
