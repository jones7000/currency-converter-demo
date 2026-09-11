import { Component, OnInit, computed, inject, signal } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatTableModule } from '@angular/material/table';

import { CurrencyApiService } from '../../core/currency-api.service';
import { toUserMessage } from '../../core/grpc-error.util';
import type { CurrencyInfo } from '../../generated/currency_pb';

/**
 * List of all available currencies, ISO code + full name. Purely a display
 * of what the backend returns and a client-side text filter over that
 * already-fetched list -- filtering isn't a business rule, it never
 * changes what leaves the backend or reaches it, so it doesn't violate the
 * dumb-client principle.
 */
@Component({
  selector: 'app-currency-overview',
  imports: [
    MatCardModule,
    MatTableModule,
    MatProgressSpinnerModule,
    MatFormFieldModule,
    MatInputModule,
    MatIconModule,
    MatButtonModule,
  ],
  templateUrl: './currency-overview.html',
  styleUrl: './currency-overview.scss',
})
export class CurrencyOverview implements OnInit {
  private readonly api = inject(CurrencyApiService);

  protected readonly currencies = signal<CurrencyInfo[]>([]);
  protected readonly loading = signal(true);
  protected readonly error = signal<string | null>(null);
  protected readonly expanded = signal(true);
  protected readonly searchTerm = signal('');
  protected readonly displayedColumns = ['code', 'fullName'];

  protected readonly filteredCurrencies = computed(() => {
    const term = this.searchTerm().trim().toLowerCase();
    const all = this.currencies();
    if (!term) {
      return all;
    }
    return all.filter(
      (currency) => currency.code.toLowerCase().includes(term) || currency.fullName.toLowerCase().includes(term),
    );
  });

  ngOnInit(): void {
    this.load();
  }

  protected toggleExpanded(): void {
    this.expanded.update((value) => !value);
  }

  protected onSearchInput(event: Event): void {
    this.searchTerm.set((event.target as HTMLInputElement).value);
  }

  private async load(): Promise<void> {
    this.loading.set(true);
    this.error.set(null);
    try {
      const response = await this.api.listCurrencies();
      this.currencies.set(response.currencies);
    } catch (err) {
      this.error.set(toUserMessage(err));
    } finally {
      this.loading.set(false);
    }
  }
}
