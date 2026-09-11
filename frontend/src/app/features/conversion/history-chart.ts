import { Component, computed, input } from '@angular/core';
import type { ChartConfiguration, ChartData } from 'chart.js';
import { BaseChartDirective } from 'ng2-charts';

import type { HistoryPoint } from '../../generated/currency_pb';

/**
 * Visual plot of the historical exchange rate over the chosen timeframe.
 * Pure rendering of what the backend returned in HistoryResponse.points --
 * no rate computation happens here.
 */
@Component({
  selector: 'app-history-chart',
  imports: [BaseChartDirective],
  templateUrl: './history-chart.html',
  styleUrl: './history-chart.scss',
})
export class HistoryChart {
  readonly points = input<HistoryPoint[]>([]);
  readonly sourceCurrency = input('');
  readonly targetCurrency = input('');

  protected readonly chartData = computed<ChartData<'line'>>(() => ({
    labels: this.points().map((point) => point.date),
    datasets: [
      {
        label: `${this.sourceCurrency()}/${this.targetCurrency()}`,
        data: this.points().map((point) => point.rate),
        fill: false,
        tension: 0.15,
        pointRadius: 2,
      },
    ],
  }));

  protected readonly chartOptions: ChartConfiguration<'line'>['options'] = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { display: true },
    },
    scales: {
      x: { title: { display: true, text: 'Date' } },
      y: { title: { display: true, text: 'Rate' } },
    },
  };
}
