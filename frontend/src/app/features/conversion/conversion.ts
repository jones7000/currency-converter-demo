import { DecimalPipe } from '@angular/common';
import { Component, OnInit, inject, signal } from '@angular/core';
import { FormBuilder, ReactiveFormsModule, Validators } from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatDatepickerModule } from '@angular/material/datepicker';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';

import { CurrencyApiService } from '../../core/currency-api.service';
import { toUserMessage } from '../../core/grpc-error.util';
import { NotificationService } from '../../core/notification.service';
import type {
  ConstraintsResponse,
  ConvertResponse,
  CurrencyInfo,
  HistoryPoint,
} from '../../generated/currency_pb';
import { HistoryChart } from './history-chart';

// The wire format is ISO 8601 (YYYY-MM-DD); the datepicker works with JS
// Date objects, so these two helpers are the only date "logic" here --
// format conversion, not a business rule.
function toDateInput(date: Date): string {
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, '0');
  const day = String(date.getDate()).padStart(2, '0');
  return `${year}-${month}-${day}`;
}

function fromDateInput(value: string): Date {
  const [year, month, day] = value.split('-').map(Number);
  return new Date(year, month - 1, day);
}

/**
 * Source/target selectors, amount, date range, and on submit the converted
 * amount + latest rate date + historical chart. Amount and date bounds
 * come from GetConstraints, never hardcoded here (dumb-client principle) --
 * the min/max attributes below are purely a UX affordance mirroring
 * server-declared values; the backend remains the sole authority and
 * re-validates every request regardless.
 */
@Component({
  selector: 'app-conversion',
  imports: [
    ReactiveFormsModule,
    DecimalPipe,
    MatCardModule,
    MatFormFieldModule,
    MatSelectModule,
    MatInputModule,
    MatDatepickerModule,
    MatIconModule,
    MatButtonModule,
    MatProgressSpinnerModule,
    HistoryChart,
  ],
  templateUrl: './conversion.html',
  styleUrl: './conversion.scss',
})
export class Conversion implements OnInit {
  private readonly api = inject(CurrencyApiService);
  private readonly fb = inject(FormBuilder).nonNullable;
  private readonly notifications = inject(NotificationService);

  protected readonly currencies = signal<CurrencyInfo[]>([]);
  protected readonly constraints = signal<ConstraintsResponse | null>(null);
  protected readonly loadingLookups = signal(true);
  protected readonly lookupError = signal<string | null>(null);

  protected readonly submitting = signal(false);
  protected readonly result = signal<ConvertResponse | null>(null);
  protected readonly historyPoints = signal<HistoryPoint[]>([]);

  protected minDate: Date | null = null;
  protected maxDate: Date | null = null;

  protected readonly form = this.fb.group({
    sourceCurrency: ['', Validators.required],
    targetCurrency: ['', Validators.required],
    amount: this.fb.control<number>(100, Validators.required),
    startDate: this.fb.control<Date | null>(null, Validators.required),
    endDate: this.fb.control<Date | null>(null, Validators.required),
  });

  ngOnInit(): void {
    this.loadLookups();
  }

  private async loadLookups(): Promise<void> {
    this.loadingLookups.set(true);
    this.lookupError.set(null);
    try {
      const [currenciesResponse, constraints] = await Promise.all([
        this.api.listCurrencies(),
        this.api.getConstraints(),
      ]);
      this.currencies.set(currenciesResponse.currencies);
      this.constraints.set(constraints);

      this.minDate = fromDateInput(constraints.minDate);
      this.maxDate = fromDateInput(constraints.maxDate);

      this.form.controls.amount.setValidators([
        Validators.required,
        Validators.min(constraints.minAmount),
        Validators.max(constraints.maxAmount),
      ]);
      this.form.controls.amount.updateValueAndValidity();
      this.form.patchValue({ startDate: this.minDate, endDate: this.maxDate });
    } catch (err) {
      this.lookupError.set(toUserMessage(err));
    } finally {
      this.loadingLookups.set(false);
    }
  }

  protected async submit(): Promise<void> {
    if (this.form.invalid) {
      this.form.markAllAsTouched();
      return;
    }

    const { sourceCurrency, targetCurrency, amount, startDate, endDate } = this.form.getRawValue();
    if (!startDate || !endDate) {
      return;
    }

    this.submitting.set(true);
    this.result.set(null);
    this.historyPoints.set([]);
    try {
      const [convertResponse, historyResponse] = await Promise.all([
        this.api.convert(sourceCurrency, targetCurrency, amount),
        this.api.getHistory(sourceCurrency, targetCurrency, toDateInput(startDate), toDateInput(endDate)),
      ]);
      this.result.set(convertResponse);
      this.historyPoints.set(historyResponse.points);
    } catch (err) {
      this.notifications.showError(toUserMessage(err));
    } finally {
      this.submitting.set(false);
    }
  }
}
